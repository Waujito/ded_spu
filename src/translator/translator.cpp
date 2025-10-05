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
			if (*subtoken != '\0')
				n_args++;

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

static int mov_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);
	
	if (ctx->n_args != 1 + 2) {
		return S_FAIL;
	}	

	int ret = 0;

	spu_register_num_t rd = 0;
	spu_register_num_t rn = 0;

	if ((ret = parse_register(ctx->argsptrs[1])) < 0) {
		log_error("Error while parsing register <%s>", ctx->argsptrs[1]);
		return S_FAIL;
	}
	rd = (spu_register_num_t)ret;

	if ((ret = parse_register(ctx->argsptrs[2])) < 0) {
		log_error("Error while parsing register <%s>", ctx->argsptrs[2]);
		return S_FAIL;
	}
	rn = (spu_register_num_t)ret;

	if (raw_cmd(ctx, instr)) {
		return S_FAIL;
	}
	if (instr_set_register(rd, instr, 0, 1)) {
		log_error("Error while setting register rd");
		return S_FAIL;
	}
	if (instr_set_bitfield(0, 2, instr, 4)) {
		log_error("Error while setting mov modificator");
		return S_FAIL;
	}
	if (instr_set_register(rn, instr, 4 + 2, 0)) {
		log_error("Error while setting register rn");
		return S_FAIL;
	}

	return S_OK;
}

static int ldr_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);	
	
	if (ctx->n_args != 1 + 2) {
		return S_FAIL;
	}

	int ret = 0;

	spu_register_num_t rd = 0;

	if ((ret = parse_register(ctx->argsptrs[1])) < 0) {
		log_error("Error while parsing register <%s>", ctx->argsptrs[1]);
		return S_FAIL;
	}
	rd = (spu_register_num_t)ret;

	int32_t number = 0;
	if (parse_literal_number(ctx->argsptrs[2], &number)) {
		log_error("Error while parsing number <%s>", ctx->argsptrs[2]);
		return S_FAIL;
	}

	if (number < 0 || number >= (1 << LDR_INTEGER_LEN)) {
		log_error("number <%s> is too long or negative", ctx->argsptrs[2]);
		return S_FAIL;
	}
	uint32_t arg_num = (uint32_t)number;

	if (raw_cmd(ctx, instr)) {
		return S_FAIL;
	}
	if (instr_set_register(rd, instr, 0, 1)) {
		log_error("Error while setting register rd");
		return S_FAIL;
	}
	if (instr_set_bitfield(arg_num, LDR_INTEGER_LEN, instr, 4)) {
		log_error("Error while setting number");
		return S_FAIL;
	}

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

	if (instr_set_bitfield(directive_opcode, 10, instr, 0)) {
		log_error("Error while directive opcode");
		return S_FAIL;
	}

	return S_OK;
}

const static struct op_cmd op_data[] = {
	{"mov",		MOV_OPCODE,	mov_cmd},
	{"ldr",		LDR_OPCODE,	ldr_cmd},
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
