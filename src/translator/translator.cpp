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
#include "opls.h"

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
	_CT_FAIL_NONZERO(pvector_read_lines(&lines_arr,
			  textbuf, textbuf_len));

	_CT_FAIL_NONZERO(pvector_init(instr_lines_arr, sizeof(struct instruction_line)));
	_CT_FAIL_NONZERO(pvector_set_capacity(instr_lines_arr, lines_arr.len + 1));

	for (size_t i = 0; i < lines_arr.len; i++) {
		struct text_line *line = NULL;
		_CT_FAIL_NONZERO(pvector_get(
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

		_CT_FAIL_NONZERO(pvector_push_back(
			instr_lines_arr, &instr_line));
	}

_CT_EXIT_POINT:
	pvector_destroy(&lines_arr);

	return ret;
}


struct label_instance {
	char label[LABEL_MAX_LEN];
	ssize_t instruction_ptr;
};

struct translating_context {
	// Input file with asm text
	struct pvector instr_lines_arr;

	// An array with binary instructions
	struct pvector instructions_arr;	

	size_t n_instruction;

	FILE *out_stream;

	// Table of labels
	struct pvector labels_table;

	int second_compilation;
};

/*

static int raw_cmd(struct asm_instruction *asm_instr,
		   struct spu_instruction *bin_instr) {
	assert (asm_instr);
	assert (bin_instr);

	unsigned int opcode = asm_instr->op_cmd->opcode;
	assert (opcode <= MAX_BASE_OPCODE);

	bin_instr->opcode.code	= opcode;
	bin_instr->opcode.reserved1 = 0;

	return S_OK;
}

static int mov_cmd(struct asm_instruction *asm_instr,
		   struct spu_instruction *bin_instr) {
	assert (asm_instr);
	assert (bin_instr);
	
	if (asm_instr->n_args != 1 + 2) {
		return S_FAIL;
	}	

	int ret = S_OK;

	spu_register_num_t dest = 0;
	spu_register_num_t src = 0;

	_CT_CHECKED(parse_register(asm_instr->argsptrs[1], &dest));
	_CT_CHECKED(parse_register(asm_instr->argsptrs[2], &src));

	_CT_CHECKED(raw_cmd(asm_instr, bin_instr));
	_CT_CHECKED(instr_set_register(dest, bin_instr,
				0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_set_bitfield(0, MOV_RESERVED_FIELD_LEN,
				bin_instr, FREGISTER_BIT_LEN));
	_CT_CHECKED(instr_set_register(src, bin_instr,
				FREGISTER_BIT_LEN + MOV_RESERVED_FIELD_LEN, NO_R_HEAD_BIT));

_CT_EXIT_POINT:
	return ret;
}

static int ldc_cmd(struct asm_instruction *asm_instr,
		   struct spu_instruction *bin_instr) {
	assert (asm_instr);	
	assert (bin_instr);	
	
	if (asm_instr->n_args != 1 + 2) {
		return S_FAIL;
	}

	int ret = S_OK;

	spu_register_num_t dest = 0;
	int32_t number = 0;
	uint32_t arg_num = 0;

	_CT_CHECKED(parse_register(asm_instr->argsptrs[1], &dest));
	_CT_CHECKED(parse_literal_number(asm_instr->argsptrs[2], &number));

	if (test_integer_bounds(number, LDC_INTEGER_BLEN)) {
		log_error("number <%s> is too long", asm_instr->argsptrs[2]);
		_CT_FAIL();
	}

	arg_num = (uint32_t)number;

	_CT_CHECKED(raw_cmd(asm_instr, bin_instr));
	_CT_CHECKED(instr_set_register(dest, bin_instr,
				0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_set_bitfield(arg_num, LDC_INTEGER_BLEN,
				bin_instr, FREGISTER_BIT_LEN));

_CT_EXIT_POINT:
	return ret;
}

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

static int parse_jmp_position(	struct asm_instruction *asm_instr,
				struct spu_instruction *bin_instr,
				const char *jmp_str, uint32_t *jmp_arg) {
	assert (asm_instr);
	assert (bin_instr);
	assert (jmp_str);
	assert (jmp_arg);

	int ret = S_OK;

	uint32_t arg_num = 0;
	int32_t relative_jmp = 0;

	if (*jmp_str == '.') {
		struct label_instance *label_inst = find_label(asm_instr->ctx, jmp_str);
		if (!label_inst) {
			size_t label_len = strlen(jmp_str);
			struct label_instance label = {{0}};

			if (label_len > LABEL_MAX_LEN) {
				log_error("Invalid label: %s", jmp_str);
				_CT_FAIL();
			}

			memcpy(label.label, jmp_str, label_len);
			label.instruction_ptr = -1;

			return S_OK;
		}

		if (asm_instr->ctx->second_compilation && label_inst->instruction_ptr == -1) {
			log_error("Undefined label: %s", jmp_str);
			_CT_FAIL();
		}

		int32_t absolute_ptr = (int32_t)label_inst->instruction_ptr;
		int32_t current_ptr = (int32_t)asm_instr->ctx->n_instruction;
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

static int jmp_cmd(struct asm_instruction *asm_instr,
		   struct spu_instruction *bin_instr) {
	assert (asm_instr);
	assert (bin_instr);
	
	if (asm_instr->n_args != 1 + 1) {
		return S_FAIL;
	}

	char *jmp_str = asm_instr->argsptrs[1];

	int ret = S_OK;

	unsigned int jump_condition = asm_instr->op_cmd->opcode;

	uint32_t arg_num = 0;

	_CT_CHECKED(parse_jmp_position(asm_instr, bin_instr, jmp_str, &arg_num));

	bin_instr->opcode.code	= JMP_OPCODE;
	bin_instr->opcode.reserved1 = 0;

	// !!! Setts flags instead of registers !!!
	_CT_CHECKED(instr_set_register(
		(spu_register_num_t)jump_condition,
		bin_instr, 0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_set_bitfield(arg_num, JMP_INTEGER_BLEN,
				bin_instr, FREGISTER_BIT_LEN));

_CT_EXIT_POINT:
	return ret;
}

static int call_cmd(struct asm_instruction *asm_instr,
		    struct spu_instruction *bin_instr) {
	assert (asm_instr);
	assert (bin_instr);
	
	if (asm_instr->n_args != 1 + 1) {
		return S_FAIL;
	}

	char *jmp_str = asm_instr->argsptrs[1];

	int ret = S_OK;

	uint32_t arg_num = 0;

	_CT_CHECKED(parse_jmp_position(asm_instr, bin_instr, jmp_str, &arg_num));

	_CT_CHECKED(raw_cmd(asm_instr, bin_instr));

	_CT_CHECKED(instr_set_bitfield(arg_num, JMP_INTEGER_BLEN,
				bin_instr, FREGISTER_BIT_LEN));

_CT_EXIT_POINT:
	return ret;
}
*/

/*
static int process_label(struct asm_instruction *asm_instr) {
	assert (asm_instr);

	int ret = S_OK;
	char *label_name = asm_instr->argsptrs[0];
	size_t label_len = strlen(label_name);
	struct label_instance label = {{0}};
	struct label_instance *found_label = NULL;

	size_t instr_ptr = asm_instr->ctx->n_instruction;

	if (	asm_instr->n_args != 1 || 
		*label_name != '.' || 
		// label_min_len + ':'
		label_len < LABEL_MIN_LEN + 1 ||
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

	if ((found_label = find_label(asm_instr->ctx, label_name))) {
		if (found_label->instruction_ptr != -1) {
			log_error("Label %s is already used", label_name);
			_CT_FAIL();
		} else {
			found_label->instruction_ptr = (ssize_t)instr_ptr;
		}
	}

	memcpy(label.label, label_name, label_len);
	label.instruction_ptr = (ssize_t)instr_ptr;

	_CT_FAIL_NONZERO(pvector_push_back(&asm_instr->ctx->labels_table, &label));

_CT_EXIT_POINT:
	return ret;
}
*/


static const struct op_cmd *find_op_cmd(const char *cmd_name) {
	const struct op_cmd *op_cmd_ptr = op_table;

	while (op_cmd_ptr->cmd_name != NULL) {
		if (!strcmp(cmd_name, op_cmd_ptr->cmd_name)) {
			return op_cmd_ptr;
		}
		op_cmd_ptr++;
	}

	return NULL;
}


static int lookup_asm_instruction(size_t n_args, char **argsptrs,
				  struct asm_instruction *asm_instr) {
	assert (argsptrs);
	assert (asm_instr);

	asm_instr->n_args	= n_args;
	asm_instr->argsptrs = argsptrs;

	if (asm_instr->n_args == 0) {
		asm_instr->is_empty = 1;
		return S_OK;
	}
	
	char *cmd = asm_instr->argsptrs[0];

	// Some instructions accept arguments divided by function name and point
	char *point_arg = strchr(cmd, '.');

	// If point is placed in start of instruction, it is label declaration
	if (cmd == point_arg) {
		asm_instr->is_label = 1;
		return S_OK;
	}

	if (point_arg) {
		*(point_arg++) = '\0';

		if (*point_arg == '\0') {
			return S_FAIL;
		}
	}


	const struct op_cmd *op_cmd_ptr = find_op_cmd(cmd);
	if (!op_cmd_ptr) {
		return S_FAIL;
	}

	asm_instr->op_cmd = op_cmd_ptr;
	asm_instr->op_arg = point_arg;

	return S_OK;
}

static int assemble_instruction(struct asm_instruction *asm_instr) {
	assert (asm_instr);

	struct spu_instruction bin_instr = {0};

	int ret = S_OK;

	const struct op_cmd *op_cmd = asm_instr->op_cmd;

	const struct op_layout *oplt = find_op_layout(op_cmd->layout);

	struct spu_instr_data instr_data = {0};

	if (!oplt) {
		_CT_FAIL();
	}

	_CT_CHECKED(oplt->parse_asm_fn(asm_instr, &instr_data));
	_CT_CHECKED(oplt->write_bin_fn(&instr_data, &bin_instr));

#ifdef T_DEBUG
	eprintf("Writing instruction: <0x");
	buf_dump_hex(&bin_instr, sizeof (bin_instr), stderr);
	eprintf(">\n");
#endif /* T_DEBUG */

	_CT_FAIL_NONZERO(pvector_push_back(
		&asm_instr->ctx->instructions_arr, &bin_instr));

	asm_instr->ctx->n_instruction++;

_CT_EXIT_POINT:
	return ret;
}

static int assembly(struct translating_context *ctx) {
	assert (ctx);

	int ret = S_OK;

	for (size_t nline = 0; nline < ctx->instr_lines_arr.len; nline++) {
		struct instruction_line *instr_line = NULL;
		_CT_FAIL_NONZERO(pvector_get(&ctx->instr_lines_arr,
			  nline, (void **)&instr_line));

		struct asm_instruction asm_instr = {0};

		if (lookup_asm_instruction(instr_line->n_args, 
				instr_line->argsptrs, &asm_instr)) {
			log_error("Invalid line #%zu", nline + 1);
			_CT_FAIL();
		}

		asm_instr.ctx = ctx;

		if (asm_instr.is_empty) {
			continue;
		} else if (asm_instr.is_label) {
			// _CT_CHECKED(process_label(&asm_instr));
		} else {
			if (assemble_instruction(&asm_instr)) {
				log_error("Invalid line #%zu", nline + 1);
				_CT_FAIL();
			}
		}
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
		.second_compilation = 0,
	};

	_CT_CHECKED(read_file(in_filename, &textbuf, &textbuf_len));

	_CT_CHECKED(tokenize_instructions(&ctx.instr_lines_arr,
			textbuf, textbuf_len));
	
	_CT_FAIL_NONZERO(pvector_init(&ctx.labels_table,
			  sizeof(struct label_instance)));
	_CT_FAIL_NONZERO(pvector_init(&ctx.instructions_arr,
			  sizeof(spu_instruction_t)));

	_CT_CHECKED(assembly(&ctx));

	ctx.second_compilation = 1;
	_CT_FAIL_NONZERO(pvector_empty(&ctx.instructions_arr));
	ctx.n_instruction = 0;
	_CT_CHECKED(assembly(&ctx));

	for (size_t i = 0; i < ctx.instructions_arr.len; i++) {
		spu_instruction_t *bin_instr = NULL;
		_CT_FAIL_NONZERO(pvector_get(
			&ctx.instructions_arr, i, (void **)&bin_instr));
		fwrite(bin_instr, sizeof(*bin_instr), 1, out_stream);
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
