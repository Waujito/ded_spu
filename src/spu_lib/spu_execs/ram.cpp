#include <assert.h>
#include "spu_asm.h"
#include "spu.h"
#include "math.h"

OP_EXEC_FN(ram_exec) {
	int64_t mem_idx = ctx->registers[instr.rdest];
	if (mem_idx < 0 || mem_idx >= RAM_SIZE) {
		return S_FAIL;
	}
	int64_t *sreg = &ctx->registers[instr.rsrc1];

	switch (instr.opcode) {
		case STM_OPCODE:
			// eprintf("%016lx\n", *sreg);
			ctx->ram[mem_idx] = *sreg;
			break;
		case LDM_OPCODE:
			*sreg = ctx->ram[mem_idx];
			// eprintf("%016lx\n", *sreg);
			break;
		default:
			return S_FAIL;
	}

	return S_OK;
}

OP_EXEC_FN(scrhw_exec) {
	ctx->registers[instr.rdest] = (int64_t) ctx->screen_height;
	ctx->registers[instr.rsrc1] = (int64_t) ctx->screen_width;

	return S_OK;
}

OP_EXEC_FN(draw_exec) {
	uint64_t mem_addr = (uint64_t) ctx->registers[instr.rdest];
	if (mem_addr > RAM_SIZE) {
		return S_FAIL;
	}

	size_t scr_len = ctx->screen_height * ctx->screen_width;
	size_t scr_mem_len = scr_len / sizeof(*ctx->ram) + 1;
	if (mem_addr + scr_mem_len > RAM_SIZE) {
		return S_FAIL;
	}

	char *vmem = (char *)(ctx->ram + mem_addr);

	for (size_t i = 0; i < scr_len; i++) {
		char el = vmem[i];

		if (i != 0 && i % ctx->screen_height == 0) {
			printf("\n");
		}

		if (el) {
			printf("* ");
		} else {
			printf(". ");
		}
	}
	printf("\n");
	
	return S_OK;
}
