#include <assert.h>
#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "opls.h"
#include "translator_parsers.h"
#include "spu.h"
#include "math.h"


OP_EXEC_FN(ldc_exec) {
	ctx->registers[instr.rdest] = instr.snum;

	return 0;
}

OP_EXEC_FN(noarg_exec) {
	switch (instr.opcode) {
		case HALT_OPCODE:
			return S_HALT;
		case DUMP_OPCODE:
			SPUDump(ctx, stdout);
			break;
		default:
			return S_FAIL;
	}
	return 0;
}

OP_EXEC_FN(arithm_unary_exec) {
	switch (instr.opcode) {
		case MOV_OPCODE:
			ctx->registers[instr.rdest] = ctx->registers[instr.rsrc1];
			break;
		case SQRT_OPCODE:
			ctx->registers[instr.rdest] = 
				(int64_t) sqrt((double)ctx->registers[instr.rsrc1]);
			break;
		default:
			return S_FAIL;
	}

	return S_OK; 
}

OP_EXEC_FN(arithm_binary_exec) {
	switch (instr.opcode) {
#define OPERATION_CASE(opcode, operation)	\
	case opcode:				\
	ctx->registers[instr.rdest] =		\
		ctx->registers[instr.rsrc1] operation ctx->registers[instr.rsrc2]; \
		break

		OPERATION_CASE(ADD_OPCODE, +);
		OPERATION_CASE(MUL_OPCODE, *);
		OPERATION_CASE(SUB_OPCODE, -);
		case DIV_OPCODE:
			if (ctx->registers[instr.rsrc2] == 0) {
				return S_FAIL;
			}
			ctx->registers[instr.rdest] = 
				ctx->registers[instr.rsrc1] /
				ctx->registers[instr.rsrc2];
			break;
		case MOD_OPCODE:
			if (ctx->registers[instr.rsrc2] == 0) {
				return S_FAIL;
			}
			ctx->registers[instr.rdest] = 
				ctx->registers[instr.rsrc1] %
				ctx->registers[instr.rsrc2];
			break;
#undef OPERATION_CASE
		
		default:
			return S_FAIL;
	}

	return S_OK;
}
