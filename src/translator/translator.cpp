#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "types.h"
#include "spu_debug.h"
#include "spu.h"
// #include "translator/address.h"

#ifdef _DEBUG
#define T_DEBUG
#endif

#define DEBUG_SETBITFIELD
#undef DEBUG_SETBITFIELD

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

static inline ssize_t instr_set_bitfield(
	uint32_t field, size_t fieldlen,
	struct spu_instruction *instr, size_t pos) {
	assert (instr);

	if (pos + fieldlen > SPU_INSTR_ARG_BITLEN) {
		return -1;
	}

	uint32_t mask = (1 << (fieldlen)) - 1;

	uint32_t arg = instr->arg;

#ifdef DEBUG_SETBITFIELD
	eprintf("old arg value: <0x");
	buf_dump_hex(&arg, 4, stderr);
	eprintf(">\n");
#endif /* DEBUG_SETBITFIELD */

	// Shift because of 24-bit field
#if __BYTE_ORDER == __LITTLE_ENDIAN
	arg <<= 8;
#endif
	arg = ntohl(arg);

	size_t shift = SPU_INSTR_ARG_BITLEN - fieldlen - pos;

#ifdef DEBUG_SETBITFIELD
	eprintf("Set bitfield in 3-byte arg "
		"with position %lu and length %lu; "
		"field value is <0x%0x>\n", pos, fieldlen, field);

	eprintf("mask: <0x%0x>; shift: <%zu>; ", mask, shift);	
#endif /* DEBUG_SETBITFIELD */

	uint32_t shifted_field = (((uint32_t)field) << shift);

#ifdef DEBUG_SETBITFIELD
	eprintf("shifted field: <0x%x>\n", shifted_field);
#endif /* DEBUG_SETBITFIELD */

	arg &= (uint32_t)(~(mask << shift));
	arg |= (shifted_field);

	arg = htonl(arg);

#ifdef DEBUG_SETBITFIELD
	eprintf("new arg value: <0x");
	buf_dump_hex(&arg, 4, stderr);
	eprintf(">\n");
#endif /* DEBUG_SETBITFIELD */

	// Shift because of 24-bit field
#if __BYTE_ORDER == __LITTLE_ENDIAN
	arg >>= 8;
#endif
	instr->arg = arg;

	return S_OK;
}

/**
 * pos in bits, [0; 23] accepted
 */ 
static ssize_t instr_set_register(spu_register_num_t rn,
			struct spu_instruction *instr, 
			size_t pos, int set_head_bit) {
	assert (instr);

	unsigned int use_head_bit = (set_head_bit != 0);

	size_t reg_copy_len = REGISTER_BIT_LEN - use_head_bit;

	if (use_head_bit) {
		instr->opcode.reg_extended = (
			(rn & REGISTER_HIGHBIT_MASK) == REGISTER_HIGHBIT_MASK);
		rn &= (~REGISTER_HIGHBIT_MASK);
	}

	if (instr_set_bitfield(rn, reg_copy_len, instr, pos)) {
		return S_FAIL;
	}

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
		str--;

		if (!strcmp(str, REGISTER_RSP_NAME)) {
			return REGISTER_RSP_CODE;
		}

		return S_FAIL;
	}

	if (rnum > 30) {
		return S_FAIL;
	}

	return (int) rnum;
}

static int parse_literal_number(const char *str, int32_t *num) {
	assert (str);
	assert (num);

	if (*str != '#') {
		return S_FAIL;
	}
	str++;

	char *endptr = NULL;
	int32_t rnum = (int32_t)strtol(str, &endptr, 0);
	if (!(*str != '\0' && *endptr == '\0')) {
		return S_FAIL;
	}

	*num = rnum;

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

		if ((n_args = tokenize_opcodeline(lineptr, argsptrs)) < 0) {
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

#ifdef T_DEBUG
		eprintf("Writing instruction: <0x");
		buf_dump_hex(&instr, sizeof (instr), stderr);
		eprintf(">\n");
#endif /* T_DEBUG */

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
