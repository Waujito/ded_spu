#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "spu.h"
#include "spu_bit_ops.h"
#include "spu_debug.h"

#define PRINT_INSTRUCTION_DISASM(instr, ...)					\
	buf_dump_hex(&instr.instruction, sizeof(instr.instruction), stdout);	\
	printf("\t");								\
	printf(__VA_ARGS__);							\
	printf("\n")

int DisasmDirectiveInstruction(struct spu_context *ctx, struct spu_instruction instr) {
	assert (ctx);
	assert (instr.opcode.code == DIRECTIVE_OPCODE);

	uint32_t directive_code = 0;
	instr_get_bitfield(&directive_code, 10, &instr, 0);

	switch (directive_code) {
		case DUMP_OPCODE:
			PRINT_INSTRUCTION_DISASM(instr, "dump");
			break;
		case HALT_OPCODE:
			PRINT_INSTRUCTION_DISASM(instr, "halt");
			break;
		default:
			PRINT_INSTRUCTION_DISASM(instr, "unknown directive : %u",
				directive_code);
			return S_FAIL;
	}

	return S_OK;
}

int DisasmInstruction(struct spu_context *ctx, struct spu_instruction instr) {
	assert (ctx);

	uint32_t num = 0;
	spu_register_num_t rd = 0;
	spu_register_num_t rn = 0;

	switch (instr.opcode.code) {
		case MOV_OPCODE:
			instr_get_register(&rd, &instr, 0, 1);
			instr_get_register(&rn, &instr, 4 + 2, 0);
			PRINT_INSTRUCTION_DISASM(instr, "mov r%d r%d", rd, rn);
			break;
		case LDR_OPCODE:
			instr_get_register(&rd, &instr, 0, 1);
			instr_get_bitfield(&num, 20, &instr, 4);
			PRINT_INSTRUCTION_DISASM(instr, "ldr r%d $0x%x", rd, num);
			break;
		case DIRECTIVE_OPCODE:
			DisasmDirectiveInstruction(ctx, instr);
			break;
		default:
			PRINT_INSTRUCTION_DISASM(instr, "Unknown %d", instr.opcode.code);
	}

	return S_OK;
}

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
	const char *binary_filename = "uwu.o";

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
