/**
 * @file
 *
 * @brief A translator
 */

#include <string.h>
#include <stdlib.h>

#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "spu_debug.h"
#include "translator_parsers.h"
#include "pvector.h"
#include "ctio.h"

#ifdef _DEBUG
#define T_DEBUG
#endif

static inline int is_comment_string(char *arg) {
	assert (arg);

	char *comment_ptr = strpbrk(arg, ";#");
	if (comment_ptr) {
		*comment_ptr = '\0';
		return 1;
	}

	return 0;
}

static const char *const argsplit_syms = " \n\t";

static ssize_t tokenize_opcodeline(char *lineptr, char *argsptrs[MAX_INSTR_ARGS]) {
	assert (lineptr);
	assert (argsptrs);

	char *saveptr = NULL;

	ssize_t n_args = 0;
	char *strtok_ptr = lineptr;
	for (; n_args < MAX_INSTR_ARGS; 
			strtok_ptr = NULL, n_args++) {
		char *subtoken = strtok_r(strtok_ptr,
			    argsplit_syms, &saveptr);
		if (subtoken == NULL)
			break;

		argsptrs[n_args] = subtoken;

		if (is_comment_string(subtoken)) {
			if (*subtoken != '\0') {
				n_args++;
			}

			break;
		}
	}

	if (n_args == MAX_INSTR_ARGS) {
		return S_FAIL;
	}

	return n_args;
}

struct instruction_line {
	char *lineptr;
	size_t linesz;
	char *argsptrs[MAX_INSTR_ARGS];
	size_t n_args;
};

static int tokenize_instructions(struct pvector *instr_lines_arr,
				 char *textbuf, size_t textbuf_len) {
	int ret = S_OK;

	PVECTOR_CREATE(lines_arr, sizeof(struct text_line));
	_CT_CHECKED(pvector_read_lines(&lines_arr, 
			  textbuf, textbuf_len));

	_CT_CHECKED(pvector_init(instr_lines_arr, sizeof(struct instruction_line)));
	_CT_CHECKED(pvector_set_capacity(instr_lines_arr, lines_arr.len + 1)); 

	for (size_t i = 0; i < lines_arr.len; i++) {
		struct text_line *line = NULL;
		_CT_CHECKED(pvector_get(
			&lines_arr, i, (void **)&line));
		struct instruction_line instr_line = {
			.lineptr = line->line_ptr,
			.linesz = line->line_sz,
			.argsptrs = {0},
			.n_args = 0,
		};

		ssize_t n_args = 0;

		if ((n_args = tokenize_opcodeline(
			instr_line.lineptr,
			instr_line.argsptrs)) < 0) {
			log_error("Invalid line #%zu", i + 1);
			_CT_FAIL();
		}
		instr_line.n_args = (size_t)n_args;

		_CT_CHECKED(pvector_push_back(
			instr_lines_arr, &instr_line));
	}

_CT_EXIT_POINT:
	pvector_destroy(&lines_arr);

	return ret;
}

typedef int(*op_parsing_fn)(struct translating_context *ctx,
			    struct spu_instruction *instr);

struct op_cmd {
	const char *cmd_name;
	unsigned int opcode;
	op_parsing_fn fun;
	op_parsing_fn disasm_fun;
};

struct label_instance {
	char label[LABEL_MAX_LEN];
	ssize_t instruction_ptr;
};

struct translating_context {
	// Input file with asm text
	pvector instr_lines_arr;

	// An array with binary instructions
	pvector instructions_arr;	

	size_t n_instruction;

	// const struct instruction_line *instr_line;
	size_t n_args;
	char **argsptrs;

	FILE *out_stream;

	// Table of labels
	pvector labels_table;

	const struct op_cmd *op_data;
	int second_compilation = 1;
	int do_second_compilation = 1;
};

static struct label_instance *find_label(struct translating_context *ctx,
					 const char *label_name) {
	assert (ctx);
	assert (label_name);

	for (size_t i = 0; i < ctx->labels_table.len; i++) {
		struct label_instance *flabel = NULL;
		if (pvector_get(&(ctx->labels_table), i, (void **)&flabel)) {
			return NULL;
		}

		if (!strcmp(flabel->label, label_name)) {
			return flabel;
		}
	}

	return NULL;
}

static int process_label(struct translating_context *ctx) {
	assert (ctx);

	int ret = S_OK;
	char *label_name = ctx->argsptrs[0];
	size_t label_len = strlen(label_name);
	struct label_instance label = {{0}};
	struct label_instance *found_label = NULL;

	if (	ctx->n_args != 1 || 
		*label_name != '.' || 
		label_len < 3 ||
		label_name[label_len - 1] != ':') {
		log_error("Invalid label: %s", label_name);
		_CT_FAIL();
	}

	label_name[label_len - 1] = '\0';
	label_len--;

	if (label_len > LABEL_MAX_LEN) {
		log_error("Label %s is too long: maximum size is %d",
			label_name, LABEL_MAX_LEN);
		_CT_FAIL();
	}

	if ((found_label = find_label(ctx, label_name))) {
		if (found_label->instruction_ptr != -1) {
			log_error("Label %s is already used", label_name);
			_CT_FAIL();
		} else {
			found_label->instruction_ptr = (ssize_t)ctx->n_instruction;
		}
	}

	memcpy(label.label, label_name, label_len);
	label.instruction_ptr = (ssize_t)ctx->n_instruction;

	if (pvector_push_back(&ctx->labels_table, &label)) {
		_CT_FAIL();
	}

_CT_EXIT_POINT:
	return ret;
}


static int raw_cmd(struct translating_context *ctx, struct spu_instruction *instr) {
	assert (ctx);
	assert (instr);

	unsigned int opcode = ctx->op_data->opcode;
	assert (opcode <= MAX_BASE_OPCODE);

	instr->opcode.code	= opcode;
	instr->opcode.reserved1 = 0;

	return S_OK;
}

static int directive_cmd(struct translating_context *ctx,
			 struct spu_instruction *instr) {
	assert (ctx);
	assert (instr);

	unsigned int opcode = DIRECTIVE_OPCODE;

	instr->opcode.code	= opcode;
	instr->opcode.reserved1 = 0;

	unsigned int directive_opcode = ctx->op_data->opcode;
	assert (opcode <= MAX_DIRECTIVE_OPCODE);

	if (instr_set_bitfield(directive_opcode, DIRECTIVE_INSTR_BITLEN, instr, 0)) {
		log_error("Error while setting directive opcode");
		return S_FAIL;
	}

	return S_OK;
}

static int mov_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);
	
	if (ctx->n_args != 1 + 2) {
		return S_FAIL;
	}	

	int ret = S_OK;

	spu_register_num_t dest = 0;
	spu_register_num_t src = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &dest));
	_CT_CHECKED(parse_register(ctx->argsptrs[2], &src));

	_CT_CHECKED(raw_cmd(ctx, instr));
	_CT_CHECKED(instr_set_register(dest, instr,
				0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_set_bitfield(0, MOV_RESERVED_FIELD_LEN,
				instr, FREGISTER_BIT_LEN));
	_CT_CHECKED(instr_set_register(src, instr,
				FREGISTER_BIT_LEN + MOV_RESERVED_FIELD_LEN, NO_R_HEAD_BIT));

_CT_EXIT_POINT:
	return ret;
}

static int ldc_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);	
	
	if (ctx->n_args != 1 + 2) {
		return S_FAIL;
	}

	int ret = S_OK;

	spu_register_num_t dest = 0;
	int32_t number = 0;
	uint32_t arg_num = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &dest));
	_CT_CHECKED(parse_literal_number(ctx->argsptrs[2], &number));

	if (test_integer_bounds(number, LDC_INTEGER_BLEN)) {
		log_error("number <%s> is too long", ctx->argsptrs[2]);
		_CT_FAIL();
	}

	arg_num = (uint32_t)number;

	_CT_CHECKED(raw_cmd(ctx, instr));
	_CT_CHECKED(instr_set_register(dest, instr,
				0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_set_bitfield(arg_num, LDC_INTEGER_BLEN,
				instr, FREGISTER_BIT_LEN));


_CT_EXIT_POINT:
	return ret;
}

static int parse_jmp_position(	struct translating_context *ctx,
				struct spu_instruction *instr,
				const char *jmp_str, uint32_t *jmp_arg) {
	assert (ctx);
	assert (instr);

	int ret = S_OK;

	uint32_t arg_num = 0;
	int32_t relative_jmp = 0;

	if (*jmp_str == '.') {
		struct label_instance *label_inst = find_label(ctx, jmp_str);
		if (!label_inst) {
			size_t label_len = strlen(jmp_str);
			struct label_instance label = {{0}};

			if (label_len > LABEL_MAX_LEN) {
				log_error("Invalid label: %s", jmp_str);
				_CT_FAIL();
			}

			memcpy(label.label, jmp_str, label_len);
			label.instruction_ptr = -1;

			ctx->do_second_compilation = 1;

			return S_OK;
		}

		if (ctx->second_compilation && label_inst->instruction_ptr == -1) {
			log_error("Undefined label: %s", jmp_str);
			_CT_FAIL();
		}

		int32_t absolute_ptr = (int32_t)label_inst->instruction_ptr;
		int32_t current_ptr = (int32_t)ctx->n_instruction;
		int32_t relative_ptr = absolute_ptr - current_ptr - 1;

		relative_jmp = relative_ptr;
	} else if (*jmp_str == '$') {
		int32_t number = 0;

		_CT_CHECKED(parse_literal_number(jmp_str, &number));

		relative_jmp = number;
	} else {
		_CT_FAIL();
	}

	if (test_integer_bounds(relative_jmp, JMP_INTEGER_BLEN)) {
		log_error("jump number <%d> is too long", relative_jmp);
		_CT_FAIL();
	}
	arg_num = (uint32_t)relative_jmp;
	
	*jmp_arg = arg_num;

_CT_EXIT_POINT:
	return ret;
}

static int jmp_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);	
	
	if (ctx->n_args != 1 + 1) {
		return S_FAIL;
	}

	char *jmp_str = ctx->argsptrs[1];

	int ret = S_OK;

	unsigned int jump_condition = ctx->op_data->opcode;
	// unsigned int jump_condition = 0;//ctx->op_data->opcode;

	uint32_t arg_num = 0;

	_CT_CHECKED(parse_jmp_position(ctx, instr, jmp_str, &arg_num));

	instr->opcode.code	= JMP_OPCODE;
	instr->opcode.reserved1 = 0;

	// !!! Setts flags instead of registers !!!
	_CT_CHECKED(instr_set_register(
		(spu_register_num_t)jump_condition,
		instr, 0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_set_bitfield(arg_num, JMP_INTEGER_BLEN,
				instr, FREGISTER_BIT_LEN));

_CT_EXIT_POINT:
	return ret;
}

// 1 + 4-bit dest, 5-bit src1, 5-bit src2
static int triple_reg_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);

	if (ctx->n_args != 1 + 3) {
		return S_FAIL;
	}

	int ret = S_OK;

	spu_register_num_t dest = 0;
	spu_register_num_t src1 = 0;
	spu_register_num_t src2 = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &dest));
	_CT_CHECKED(parse_register(ctx->argsptrs[2], &src1));
	_CT_CHECKED(parse_register(ctx->argsptrs[3], &src2));

	_CT_CHECKED(directive_cmd(ctx, instr));
	_CT_CHECKED(directive_set_register(dest, instr,
				    0, USE_R_HEAD_BIT));
	_CT_CHECKED(directive_set_register(src1, instr,
				    FREGISTER_BIT_LEN, NO_R_HEAD_BIT));
	_CT_CHECKED(directive_set_register(src2, instr,
				    FREGISTER_BIT_LEN + REGISTER_BIT_LEN, NO_R_HEAD_BIT));

_CT_EXIT_POINT:
	return ret;
}

static int unary_op_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);

	if (ctx->n_args != 1 + 2) {
		return S_FAIL;
	}

	int ret = S_OK;

	spu_register_num_t dest = 0;
	spu_register_num_t src = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &dest));
	_CT_CHECKED(parse_register(ctx->argsptrs[2], &src));

	_CT_CHECKED(directive_cmd(ctx, instr));
	_CT_CHECKED(directive_set_register(dest, instr,
				    0, USE_R_HEAD_BIT));
	_CT_CHECKED(directive_set_register(src, instr,
				    FREGISTER_BIT_LEN, NO_R_HEAD_BIT));

_CT_EXIT_POINT:
	return ret;
}

static int single_reg_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);

	if (ctx->n_args != 1 + 1) {
		return S_FAIL;
	}

	int ret = S_OK;

	spu_register_num_t src = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &src));

	_CT_CHECKED(directive_cmd(ctx, instr));
	_CT_CHECKED(directive_set_register(src, instr,
				0, USE_R_HEAD_BIT));

_CT_EXIT_POINT:
	return ret;
}

const static struct op_cmd op_data[] = {
	{"mov",		MOV_OPCODE,	mov_cmd},
	{"ldc",		LDC_OPCODE,	ldc_cmd},
	{"jmp",		UNCONDITIONAL_JMP,	jmp_cmd},
	{"jmp.eq",	EQUALS_JMP,		jmp_cmd},
	{"jmp.neq",	NOT_EQUALS_JMP,		jmp_cmd},
	{"jmp.geq",	GREATER_EQUALS_JMP,	jmp_cmd},
	{"jmp.gt",	GREATER_JMP,		jmp_cmd},
	{"jmp.leq",	LESS_EQUALS_JMP,	jmp_cmd},
	{"jmp.lt",	LESS_JMP,		jmp_cmd},
	{"pushr",	PUSHR_OPCODE,	single_reg_cmd},
	{"popr",	POPR_OPCODE,	single_reg_cmd},
	{"input",	INPUT_OPCODE,	single_reg_cmd},
	{"print",	PRINT_OPCODE,	single_reg_cmd},
	{"cmp",		CMP_OPCODE,	unary_op_cmd},
	{"add",		ADD_OPCODE,	triple_reg_cmd},
	{"mul",		MUL_OPCODE,	triple_reg_cmd},
	{"sub",		SUB_OPCODE,	triple_reg_cmd},
	{"div",		DIV_OPCODE,	triple_reg_cmd},
	{"mod",		MOD_OPCODE,	triple_reg_cmd},
	{"sqrt",	SQRT_OPCODE,	unary_op_cmd},
	{"dump",	DUMP_OPCODE,	directive_cmd},
	{"halt",	HALT_OPCODE,	directive_cmd},
	{0}
};

#define S_EMPTY_INSTR (2)

static int parse_op(struct translating_context *ctx,
		    struct spu_instruction *instr) {
	assert (ctx);
	assert (instr);
	assert (ctx->argsptrs);

	if (ctx->n_args == 0) {
		return S_EMPTY_INSTR;
	}
	
	char *cmd = ctx->argsptrs[0];

	if (*cmd == '.') {
		if (!ctx->second_compilation && process_label(ctx)) {
			return S_FAIL;
		}

		return S_EMPTY_INSTR;
	}

	const struct op_cmd *op_cmd_ptr = op_data;
	while (op_cmd_ptr->cmd_name != NULL) {
		if (!strcmp(cmd, op_cmd_ptr->cmd_name)) {
			ctx->op_data = op_cmd_ptr;
			return op_cmd_ptr->fun(ctx, instr);
		}
		op_cmd_ptr++;
	}

	return S_FAIL;
}

static int assembly(struct translating_context *ctx) {
	assert (ctx);

	int ret = S_OK;

	int status = 0;

	size_t n_lines = 0;

	for (size_t i = 0; i < ctx->instr_lines_arr.len; i++) {
		struct instruction_line *instr_line = NULL;
		_CT_CHECKED(pvector_get(&ctx->instr_lines_arr,
			  i, (void **)&instr_line));

		struct spu_instruction instr = {0};

		ctx->argsptrs = instr_line->argsptrs;
		ctx->n_args = instr_line->n_args;

		if ((status = parse_op(ctx, &instr)) < 0) {
			log_error("Invalid line #%zu", n_lines);
			_CT_FAIL();
		}

		if (status != S_EMPTY_INSTR) {
#ifdef T_DEBUG
			eprintf("Writing instruction: <0x");
			buf_dump_hex(&instr, sizeof (instr), stderr);
			eprintf(">\n");
#endif /* T_DEBUG */
			ctx->n_instruction++;
			_CT_CHECKED(pvector_push_back(
				&ctx->instructions_arr, &instr));
		}

		n_lines++;
	}

_CT_EXIT_POINT:
	return ret;
}

/**
 * The in_stream should be seekable
 */
static int parse_text(const char *in_filename, FILE *out_stream) {
	assert (in_filename);
	assert (out_stream);

	int ret = S_OK;

	char *textbuf = NULL;
	size_t textbuf_len = 0;

	struct translating_context ctx = {
		.instr_lines_arr = {0},
		.instructions_arr = {0},

		.n_instruction = 0,

		.out_stream = out_stream,
		.labels_table = {0},
		.op_data = NULL,
		.second_compilation = 0,
		.do_second_compilation = 0,
	};

	_CT_CHECKED(read_file(in_filename, &textbuf, &textbuf_len));

	_CT_CHECKED(tokenize_instructions(&ctx.instr_lines_arr,
			textbuf, textbuf_len));
	
	_CT_CHECKED(pvector_init(&ctx.labels_table,
			  sizeof(struct label_instance)));
	_CT_CHECKED(pvector_init(&ctx.instructions_arr,
			  sizeof(spu_instruction_t)));

	_CT_CHECKED(assembly(&ctx));

	ctx.second_compilation = 1;
	_CT_CHECKED(pvector_empty(&ctx.instructions_arr));
	ctx.n_instruction = 0;
	_CT_CHECKED(assembly(&ctx));

	for (size_t i = 0; i < ctx.instructions_arr.len; i++) {
		spu_instruction_t *instr = NULL;
		_CT_CHECKED(pvector_get(
			&ctx.instructions_arr, i, (void **)&instr));
		fwrite(instr, sizeof(*instr), 1, out_stream);
	}

_CT_EXIT_POINT:
	pvector_destroy(&ctx.instructions_arr);
	pvector_destroy(&ctx.labels_table);
	pvector_destroy(&ctx.instr_lines_arr);
	free(textbuf);

	return ret;
}


int main(int argc, const char *argv[]) {
	const char *asm_filename = "example.asm";

	if (argc < 2) {
	} else if (argc == 2) {
		asm_filename = argv[1];
	} else {
		log_error("Invalid args");
		return EXIT_FAILURE;
	}

	if (parse_text(asm_filename, stdout)) {
		log_error("Error while parsing asm");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
