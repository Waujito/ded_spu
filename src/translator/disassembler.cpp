#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define SPU_DISASM_MODE
#include "spu.h"

#define SPU_INSTR_MODE_DISASM
#include "spu_instruction.h"

int DisasmLoop(struct spu_context *ctx) {
	assert (ctx);

	int ret = S_OK;

	while (ctx->ip < ctx->instr_bufsize) {
		struct spu_instruction instr = {
			.instruction = ctx->instr_buf[ctx->ip]
		};
		ctx->ip++;

		ret = DisasmInstruction(ctx, instr);
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

static int disasm(const char *in_filename) {
	SPUCreate(ctx);

	int ret = S_OK;

	_CT_CHECKED(SPULoadBinary(&ctx, in_filename));

	_CT_CHECKED(DisasmLoop(&ctx));

_CT_EXIT_POINT:
	SPUDtor(&ctx);
	return ret;
}

int main(int argc, const char *argv[]) {
	const char *binary_filename = "example.o";

	if (argc < 2) {
	} else if (argc == 2) {
		binary_filename = argv[1];
	} else {
		log_error("Invalid args");
		return EXIT_FAILURE;
	}

	if (disasm(binary_filename)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
