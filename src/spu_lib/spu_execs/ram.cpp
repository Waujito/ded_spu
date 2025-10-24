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
			ctx->ram[mem_idx] = *sreg;
			break;
		case LDM_OPCODE:
			*sreg = ctx->ram[mem_idx];
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
