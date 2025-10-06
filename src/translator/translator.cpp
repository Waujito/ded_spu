#include <string.h>
#include <stdlib.h>

#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "spu_debug.h"
#include "translator_parsers.h"

#ifdef _DEBUG
#define T_DEBUG
#endif

static inline int is_comment_string(char *arg) {
	assert (arg);

	char *comment_ptr = strchr(arg, ';');
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

typedef int(*op_parsing_fn)(struct translating_context *ctx,
			    struct spu_instruction *instr);

struct op_cmd {
	const char *cmd_name;
	unsigned int opcode;
	op_parsing_fn fun;
	op_parsing_fn disasm_fun;
};

struct translating_context {
	char **argsptrs;
	size_t n_args;
	FILE *out_stream;
	const struct op_cmd *op_data;
};

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

	spu_register_num_t rd = 0;
	spu_register_num_t rn = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &rd));
	_CT_CHECKED(parse_register(ctx->argsptrs[2], &rn));

	_CT_CHECKED(raw_cmd(ctx, instr));
	_CT_CHECKED(instr_set_register(rd, instr, 0, 1));
	_CT_CHECKED(instr_set_bitfield(0, MOV_RESERVED_FIELD_LEN,
				instr, FREGISTER_BIT_LEN));
	_CT_CHECKED(instr_set_register(rn, instr,
				FREGISTER_BIT_LEN + MOV_RESERVED_FIELD_LEN, 0));

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

	spu_register_num_t rd = 0;
	int32_t number = 0;
	uint32_t arg_num = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &rd));
	_CT_CHECKED(parse_literal_number(ctx->argsptrs[2], &number));

	if (number < 0 || number >= (1 << LDC_INTEGER_LEN)) {
		log_error("number <%s> is too long or negative", ctx->argsptrs[2]);
		_CT_FAIL();
	}

	arg_num = (uint32_t)number;

	_CT_CHECKED(raw_cmd(ctx, instr));
	_CT_CHECKED(instr_set_register(rd, instr, 0, 1));
	_CT_CHECKED(instr_set_bitfield(arg_num, LDC_INTEGER_LEN,
				instr, FREGISTER_BIT_LEN));


_CT_EXIT_POINT:
	return ret;
}

static int jmp_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);	
	
	if (ctx->n_args != 1 + 1) {
		return S_FAIL;
	}

	int ret = S_OK;

	int32_t number = 0;
	uint32_t arg_num = 0;

	_CT_CHECKED(parse_literal_number(ctx->argsptrs[1], &number));

	if (number < -(1 << 23) || number >= (1 << 23)) {
		log_error("number <%s> is too long", ctx->argsptrs[2]);
		_CT_FAIL();
	}

	arg_num = (uint32_t)number;

	_CT_CHECKED(raw_cmd(ctx, instr));
	_CT_CHECKED(instr_set_bitfield(arg_num, JMP_INTEGER_BLEN,
				instr, 0));

_CT_EXIT_POINT:
	return ret;
}

// 4-bit rd, 5-bit rl, 5-bit rr
static int triple_reg_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);

	if (ctx->n_args != 1 + 3) {
		return S_FAIL;
	}

	int ret = S_OK;

	spu_register_num_t rd = 0;
	spu_register_num_t rl = 0;
	spu_register_num_t rr = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &rd));
	_CT_CHECKED(parse_register(ctx->argsptrs[2], &rl));
	_CT_CHECKED(parse_register(ctx->argsptrs[3], &rr));

	_CT_CHECKED(directive_cmd(ctx, instr));
	_CT_CHECKED(directive_set_register(rd, instr,0, 1));
	_CT_CHECKED(directive_set_register(rl, instr, FREGISTER_BIT_LEN, 0));
	_CT_CHECKED(directive_set_register(rr, instr,
				    FREGISTER_BIT_LEN + REGISTER_BIT_LEN, 0));

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

	spu_register_num_t rd = 0;
	spu_register_num_t rn = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &rd));
	_CT_CHECKED(parse_register(ctx->argsptrs[2], &rn));

	_CT_CHECKED(directive_cmd(ctx, instr));
	_CT_CHECKED(directive_set_register(rd, instr, 0, 1));
	_CT_CHECKED(directive_set_register(rn, instr, FREGISTER_BIT_LEN, 0));

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

	spu_register_num_t rn = 0;

	_CT_CHECKED(parse_register(ctx->argsptrs[1], &rn));

	_CT_CHECKED(directive_cmd(ctx, instr));
	_CT_CHECKED(directive_set_register(rn, instr, 0, 1));

_CT_EXIT_POINT:
	return ret;
}

const static struct op_cmd op_data[] = {
	{"mov",		MOV_OPCODE,	mov_cmd},
	{"ldc",		LDC_OPCODE,	ldc_cmd},
	{"jmp",		JMP_OPCODE,	jmp_cmd},
	{"pushr",	PUSHR_OPCODE,	single_reg_cmd},
	{"popr",	POPR_OPCODE,	single_reg_cmd},
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

static int parse_text(FILE *in_stream, FILE *out_stream) {
	assert (in_stream);
	assert (out_stream);

	int ret = S_OK;

	char *lineptr = NULL;
	size_t linesz = 0;
	ssize_t nread = 0;

	size_t n_lines = 0;

	struct translating_context ctx = {
		.argsptrs = NULL,
		.n_args = 0,
		.out_stream = out_stream,
		.op_data = NULL,
	};

	while ((nread = getline(&lineptr, &linesz, stdin)) != -1) {
		char *argsptrs[MAX_INSTR_ARGS];
		ssize_t n_args = 0;

		if ((n_args = tokenize_opcodeline(lineptr, argsptrs)) < 0) {
			log_error("Invalid line #%zu", n_lines);
			_CT_FAIL();
		}

		ctx.argsptrs = argsptrs;
		ctx.n_args = (size_t)n_args;

		struct spu_instruction instr = {0};

		if ((ret = parse_op(&ctx, &instr)) < 0) {
			log_error("Invalid line #%zu", n_lines);
			_CT_FAIL();
		}

		if (ret != S_EMPTY_INSTR) {
#ifdef T_DEBUG
			eprintf("Writing instruction: <0x");
			buf_dump_hex(&instr, sizeof (instr), stderr);
			eprintf(">\n");
#endif /* T_DEBUG */
		
			fwrite(&instr, sizeof(instr), 1, out_stream);
		}

		n_lines++;
	}

_CT_EXIT_POINT:
	free (lineptr);
	fflush (out_stream);
	return ret;
}


int main() {
	if (parse_text(stdin, stdout)) {
		return 1;
	}

	return 0;
}
