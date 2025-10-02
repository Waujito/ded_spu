#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "types.h"
#include "spu.h"
#include "translator/address.h"

static inline int is_comment_string(char *arg) {
	assert (arg);

	char *comment_ptr = strchr(arg, ';');
	if (comment_ptr) {
		*comment_ptr = '\0';
	}

	return 0;
}

static const char *const argsplit_syms = " \n\t";

// TODO rename tokenize
static ssize_t parse_opcodeline(char *lineptr, char *argsptrs[MAX_OPCODE_ARGSLEN]) {
	assert (lineptr);
	assert (argsptrs);

	char *saveptr = NULL;

	ssize_t n_args = 0;
	char *strtok_ptr = lineptr;
	for (; n_args < MAX_OPCODE_ARGSLEN; 
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

	if (n_args == MAX_OPCODE_ARGSLEN) {
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

/**
 * pos in bits, [0; 23] accepted
 */ 
static ssize_t set_register(spu_register_num_t rn,
			struct spu_instruction *instr, 
			size_t pos, int set_head_bit) {
	assert (instr);
	if (pos >= 24) {
		return -1;
	}

	unsigned int use_head_bit = (set_head_bit != 0);

	size_t reg_copy_len = REGISTER_BIT_LEN - use_head_bit;
	if (pos + reg_copy_len >= SPU_INSTR_ARG_BITLEN) {
		return -1;
	}

	uint32_t arg = instr->arg;
	size_t shift = SPU_INSTR_ARG_BITLEN - reg_copy_len - pos;

	
	if (use_head_bit) {
		instr->opcode.reg_extended = (
			(rn & REGISTER_HIGHBIT_MASK) == REGISTER_HIGHBIT_MASK);
		rn &= (~REGISTER_HIGHBIT_MASK);
		arg &= (uint32_t)(~(REGISTER_4BIT_MASK << shift));
	} else {
		arg &= (uint32_t)(~(REGISTER_5BIT_MASK << shift));
	}

	arg |= (((uint32_t)rn) << shift);

	instr->arg = arg;
	return S_OK;
}

/**
 * Parses a register.
 * Returns the number of register or -1 on error.
 */ 
static int parse_register(const char *str) {
	if (str[0] != 'r') {
		return S_FAIL;
	}

	str++;

	char *endptr = NULL;
	unsigned long rnum = strtoul(str, &endptr, 10);
	if (!(*str != '\0' && *endptr == '\0')) {
		if (!strcmp(str, REGISTER_RSP_NAME)) {
			return REGISTER_RSP_CODE;
		}
	}

	if (rnum > 30) {
		return S_FAIL;
	}

	return (int) rnum;
}

static int mov_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);
	
	if (ctx->n_args != 1 + 2) {
		return S_FAIL;
	}

	struct spu_addrdata src = {{0}};
	struct spu_addrdata dst = {{0}};

	if (parse_address(ctx->argsptrs[1], &src) < 0) {
		log_error("Error while parsing <%s>", ctx->argsptrs[1]);
		return S_FAIL;
	}
	dump_addrdata(src);

	if (parse_address(ctx->argsptrs[2], &dst) < 0) {
		log_error("Error while parsing <%s>", ctx->argsptrs[2]);
		return S_FAIL;
	}
	dump_addrdata(dst);

	if (src.head.datalength != dst.head.datalength) {
		log_error("Data lengths does not match");
		return S_FAIL;
	}

	uint8_t cmdbuf[1 + 2 * sizeof(struct spu_addrdata)];
	size_t buflen = 0;
	size_t addrbuf_len = 0;

	cmdbuf[0] = (uint8_t)ctx->op_data->opcode;
	buflen++;

	write_raw_addr(dst, cmdbuf + buflen, &addrbuf_len);
	buflen += addrbuf_len;

	write_raw_addr(dst, cmdbuf + buflen, &addrbuf_len);
	buflen += addrbuf_len;

	fwrite(cmdbuf, 1, buflen, ctx->out_stream);

	return S_OK;
}

static int ldr_cmd(struct translating_context *ctx,
		   struct spu_instruction *instr) {
	assert (ctx);

	if (raw_cmd(ctx, instr)) {
		return S_FAIL;
	}
	
	if (ctx->n_args != 1 + 2) {
		return S_FAIL;
	}
}

const static struct op_cmd op_data[] = {
	{"mov",		MOV_OPCODE,	mov_cmd},
	{"ldr",		LDR_OPCODE,	ldr_cmd},
	{"halt",	HALT_OPCODE,	raw_cmd},
	{"dump",	DUMP_OPCODE,	raw_cmd},
	{0}
};

static int parse_op(struct translating_context *ctx,
		    struct spu_instruction *instr) {
	assert (ctx);
	assert (instr);
	assert (ctx->argsptrs);

	if (ctx->n_args == 0) {
		return S_OK;
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

int parse_text(FILE *in_stream, FILE *out_stream) {
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

		if ((n_args = parse_opcodeline(lineptr, argsptrs)) < 0) {
			log_error("Invalid line #%zu", n_lines);
			_CT_FAIL();
		}

		ctx.argsptrs = argsptrs;
		ctx.n_args = (size_t)n_args;

		struct spu_instruction instr = {0};

		if (parse_op(&ctx, &instr)) {
			log_error("Invalid line #%zu", n_lines);
			_CT_FAIL();
		}

		fwrite(&instr, sizeof(instr), 1, out_stream);

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
