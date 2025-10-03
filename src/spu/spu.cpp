#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>
#include <arpa/inet.h>

#include "types.h"
#include "spu.h"
#include "ctio.h"
#include "spu_debug.h"

#ifdef _DEBUG
#define DEBUG_INSTRUCTIONS
#endif


#ifdef DEBUG_INSTRUCTIONS
#define INSTR_DEBUG_LOG(...) eprintf(__VA_ARGS__)
#else
#define INSTR_DEBUG_LOG(...) (void)0
#endif


struct spu_context {
	uint64_t registers[N_REGISTERS];
	spu_instruction_t *instr_buf;
	size_t instr_bufsize;
	size_t ip; 
};

int dump_spu(struct spu_context *ctx, FILE *out_stream) {
	assert (ctx);

#define DPRINT(...) fprintf(out_stream, __VA_ARGS__)

	DPRINT(	"SPU Core Dumped: {\n\n" );
	for (size_t i = 0; i < 5; i++) {
		DPRINT("r%zu:\t<0x", i);
		buf_dump_hex(&ctx->registers[i], sizeof(ctx->registers[0]), out_stream);
		DPRINT(	">\n");
	}

	DPRINT("rsp:\t<0x");
	buf_dump_hex(&ctx->registers[REGISTER_RSP_CODE], 
			sizeof(ctx->registers[0]), out_stream);
	DPRINT(	">\n");

	DPRINT("ip:\t<%lx>\n", ctx->ip);

	DPRINT("\n}\n\n");

#undef DPRINT

	return S_OK;
}

static int instr_get_bitfield(
	uint32_t *field, size_t fieldlen,
	const struct spu_instruction *instr, size_t pos) {
	assert (field);
	assert (instr);

	if (pos + fieldlen > SPU_INSTR_ARG_BITLEN) {
		return -1;
	}

	uint32_t mask = (1 << (fieldlen)) - 1;

	uint32_t arg = instr->arg;

	// Shift because of 24-bit field
#if __BYTE_ORDER == __LITTLE_ENDIAN
	arg <<= 8;
#endif
	arg = ntohl(arg);

	size_t shift = SPU_INSTR_ARG_BITLEN - fieldlen - pos;

	uint32_t shifted_mask = (mask << shift);

	uint32_t shifted_field = (arg & shifted_mask);
	uint32_t real_field = shifted_field >> shift;

	*field = real_field;
	
	return S_OK;
}

static int instr_get_register(spu_register_num_t *rn,
			const struct spu_instruction *instr, 
			size_t pos, int spec_head_bit) {
	assert (rn);
	assert (instr);

	unsigned int use_head_bit = (spec_head_bit != 0);

	size_t reg_len = REGISTER_BIT_LEN - use_head_bit;

	uint32_t regnum = 0;
	if (instr_get_bitfield(&regnum, reg_len, instr, pos)) {
		return S_FAIL;
	}

	if (use_head_bit && instr->opcode.reg_extended) {
		regnum |= REGISTER_HIGHBIT_MASK;
	}

	*rn = (uint8_t)regnum;

	return S_OK;
}

int execute(struct spu_context *ctx) {
	assert (ctx);

	while (ctx->ip < ctx->instr_bufsize) {
		struct spu_instruction instr = {
			.instruction = ctx->instr_buf[ctx->ip]
		};
		ctx->ip++;

		uint32_t num = 0;
		spu_register_num_t rd = 0;
		spu_register_num_t rn = 0;

		switch (instr.opcode.code) {
			case MOV_OPCODE:
				instr_get_register(&rd, &instr, 0, 1);
				instr_get_register(&rn, &instr, 4 + 2, 0);
				INSTR_DEBUG_LOG("MOV rd: <%d>, rn: <%d>\n", rd, rn);

				ctx->registers[rd] = ctx->registers[rn];

				break;
			case LDR_OPCODE:
				instr_get_register(&rd, &instr, 0, 1);
				instr_get_bitfield(&num, 20, &instr, 4);

				INSTR_DEBUG_LOG("LDR rd: <%d>, number: <%u>\n", rd, num);

				num = htole32(num);
				ctx->registers[rd] = num;

				break;
			case DUMP_OPCODE:
				INSTR_DEBUG_LOG("DUMP\n");
				dump_spu(ctx, stdout);
				break;
			case HALT_OPCODE:
				INSTR_DEBUG_LOG("HALT\n");
				break;
			default:
				return S_FAIL;
		}
	}

	return S_OK;
}

int parse_stream(const char *in_filename) {
	spu_instruction_t *instr_buf = NULL;
	size_t instr_bufsize = 0;
	struct spu_context ctx = {0};
	int ret = S_OK;

	_CT_CHECKED(read_file(in_filename, (char **)&instr_buf, &instr_bufsize));

	instr_bufsize /= sizeof(spu_instruction_t);

	ctx = {
		.registers = {0},
		.instr_buf = instr_buf,
		.instr_bufsize = instr_bufsize,
		.ip = 0
	};

	if ((ret = execute(&ctx))) {
		log_error("The program exited with non-zero exit code: <%d>", ret);
		ret = S_OK;
	}

_CT_EXIT_POINT:
	free(instr_buf);
	return ret;
}

int main(int argc, const char *argv[]) {
	const char *binary_filename = "uwu.o";

	if (argc < 2) {
	} else if (argc == 2) {
		binary_filename = argv[1];
	} else {
		log_error("Invalid args");
		return EXIT_FAILURE;
	}

	if (parse_stream(binary_filename)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
