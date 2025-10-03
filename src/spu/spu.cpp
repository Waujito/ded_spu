#include <stdlib.h>

#include "spu.h"
#include "spu_bit_ops.h"
#include "spu_debug.h"

#include "ctio.h"

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

static int dump_spu(struct spu_context *ctx, FILE *out_stream) {
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

static int execute(struct spu_context *ctx) {
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

static int parse_stream(const char *in_filename) {
	spu_instruction_t *instr_buf = NULL;
	size_t instr_bufsize = 0;
	struct spu_context ctx = {{0}};
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
