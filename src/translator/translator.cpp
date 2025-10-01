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

	while (*arg != '\0') {
		if (*arg == ';') {
			*arg = '\0';
			return 1;
		}

		arg++;
	}

	return 0;
}

static const char *const argsplit_syms = " \n\t";

static ssize_t parse_opcodeline(char *lineptr, char *argsptrs[MAX_OPCODE_ARGSLEN]) {
	assert (lineptr);
	assert (argsptrs);

	char *saveptr = NULL;

	ssize_t n_args = 0;
	char *strtok_ptr = lineptr;
	for (; n_args < MAX_OPCODE_ARGSLEN; strtok_ptr = NULL, n_args++) {
		char *subtoken = strtok_r(strtok_ptr, argsplit_syms, &saveptr);
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

typedef int(*op_parsing_fn)(struct translating_context *ctx);

struct op_cmd {
	const char *cmd_name;
	int opcode;
	op_parsing_fn fun;
};

struct translating_context {
	char **argsptrs;
	size_t n_args;
	FILE *out_stream;
	const struct op_cmd *op_data;
};

static int mov_cmd(struct translating_context *ctx) {
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

	uint8_t addrbuf[sizeof(struct spu_addrdata)];
	size_t addrbuf_len = 0;
	write_raw_addr(dst, addrbuf, &addrbuf_len);

	return S_OK;
}

static int raw_opcode(struct translating_context *ctx) {
	assert (ctx);

	uint8_t opcode = (uint8_t)ctx->op_data->opcode;
	fwrite(&opcode, 1, 1, ctx->out_stream);

	return S_OK;
}

const static struct op_cmd op_data[] = {
	{"mov",		MOV_OPCODE,	mov_cmd},
	{"push",	PUSH_OPCODE,	raw_opcode},
	{"halt",	HALT_OPCODE,	raw_opcode},
	{"dump",	DUMP_OPCODE,	raw_opcode},
	{0}
};

static int parse_op(struct translating_context *ctx) {
	assert (ctx);
	assert (ctx->argsptrs);

	if (ctx->n_args == 0) {
		return S_OK;
	}
	
	char *cmd = ctx->argsptrs[0];

	const struct op_cmd *op_cmd_ptr = op_data;
	while (op_cmd_ptr->cmd_name != NULL) {
		if (!strcmp(cmd, op_cmd_ptr->cmd_name)) {
			ctx->op_data = op_cmd_ptr;
			return op_cmd_ptr->fun(ctx);
		}
		op_cmd_ptr++;
	}

	return S_FAIL;
}

int parse_text(FILE *in_stream) {
	assert (in_stream);

	int ret = S_OK;

	char *lineptr = NULL;
	size_t linesz = 0;
	ssize_t nread = 0;

	size_t n_lines = 0;

	while ((nread = getline(&lineptr, &linesz, stdin)) != -1) {
		char *argsptrs[MAX_OPCODE_ARGSLEN];
		ssize_t n_args = 0;

		if ((n_args = parse_opcodeline(lineptr, argsptrs)) < 0) {
			log_error("Invalid line #%zu", n_lines);
			_CT_FAIL();
		}

		struct translating_context ctx = {
			.argsptrs = argsptrs,
			.n_args = (size_t) n_args
		};

		if (parse_op(&ctx)) {
			log_error("Invalid line #%zu", n_lines);
			_CT_FAIL();
		}

		n_lines++;
	}

_CT_EXIT_POINT:
	free (lineptr);
	return ret;
}


int main() {
	if (parse_text(stdin)) {
		return 1;
	}

	return 0;
}
