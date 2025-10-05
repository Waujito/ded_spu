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


int SPUCtor(struct spu_context *ctx) {
	assert (ctx);

	*ctx = {
		.registers = {0},
		.instr_buf = NULL,
		.instr_bufsize = 0,
		.ip = 0
	};

	return S_OK;
}

int SPUDtor(struct spu_context *ctx) {
	assert (ctx);

	free (ctx->instr_buf);
	ctx->instr_buf = NULL;
	ctx->instr_bufsize = 0;

	return S_OK;
}

int SPULoadBinary(struct spu_context *ctx, const char *filename) {
	assert (ctx);
	assert (filename);

	spu_instruction_t *instr_buf = NULL;
	size_t instr_bufsize = 0;
	int ret = S_OK;

	_CT_CHECKED(read_file(filename, (char **)&instr_buf, &instr_bufsize));

	instr_bufsize /= sizeof(spu_instruction_t);

	ctx->instr_buf = instr_buf;
	ctx->instr_bufsize = instr_bufsize;
	ctx->ip = 0;

_CT_EXIT_POINT:
	return ret;
}


#define S_EXIT (3)

int SPUExecuteDirective(struct spu_context *ctx, struct spu_instruction instr) {
	assert (ctx);
	assert (instr.opcode.code == DIRECTIVE_OPCODE);

	uint32_t directive_code = 0;
	instr_get_bitfield(&directive_code, 10, &instr, 0);

	switch (directive_code) {
		case DUMP_OPCODE:
			INSTR_DEBUG_LOG("DUMP\n");
			SPUDump(ctx, stdout);
			break;
		case HALT_OPCODE:
			INSTR_DEBUG_LOG("HALT\n");
			return S_EXIT;
		default:
			INSTR_DEBUG_LOG("Unknown directive instruction: %u\n",
				directive_code);
			return S_FAIL;
	}

	return S_OK;
}

int SPUExecuteInstruction(struct spu_context *ctx, struct spu_instruction instr) {
	assert (ctx);

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
		case DIRECTIVE_OPCODE:
			INSTR_DEBUG_LOG("Directive Instruction\n");
			return SPUExecuteDirective(ctx, instr);
		default:
			INSTR_DEBUG_LOG("Unknown instruction: %d\n", instr.opcode.code);
			return S_FAIL;
	}

	return S_OK;
}

int SPUExecute(struct spu_context *ctx) {
	assert (ctx);

	int ret = S_OK;

	while (ctx->ip < ctx->instr_bufsize) {
		struct spu_instruction instr = {
			.instruction = ctx->instr_buf[ctx->ip]
		};
		ctx->ip++;

		ret = SPUExecuteInstruction(ctx, instr);
		if (ret) {
			if (ret < 0) {
				return S_FAIL;
			} else {
				return S_OK;
			}
		}
	}

	return S_OK;
}

int SPUDump(struct spu_context *ctx, FILE *out_stream) {
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
