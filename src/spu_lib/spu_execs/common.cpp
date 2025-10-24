#include <assert.h>
#include "spu_asm.h"
#include "spu.h"
#include "math.h"


OP_EXEC_FN(ldc_exec) {
	ctx->registers[instr.rdest] = instr.snum;

	return S_OK;
}

OP_EXEC_FN(cmp_exec) {
	int64_t lnum = ctx->registers[instr.rdest];
	int64_t rnum = ctx->registers[instr.rsrc1];

	ctx->RFLAGS = 0;

	if (lnum == rnum) {
		ctx->RFLAGS |= CMP_EQ_FLAG;
	}
	if (lnum < rnum) {
		ctx->RFLAGS |= CMP_SIGN_FLAG;
	}

	return S_OK;
}

OP_EXEC_FN(rpush_pop_exec) {
	int ret = S_OK;

	switch (instr.opcode) {
		case PUSHR_OPCODE:
			_CT_FAIL_NONZERO(pvector_push_back(
				&ctx->stack, &(ctx->registers[instr.rdest])));

			break;
		case POPR_OPCODE:
			_CT_FAIL_NONZERO(pvector_pop_back(
				&ctx->stack, &(ctx->registers[instr.rdest])));
			break;
		default:
			_CT_FAIL();
	}

_CT_EXIT_POINT:
	return ret;
}

OP_EXEC_FN(simple_io_exec) {
	int ret = S_OK;

	switch (instr.opcode) {
		case INPUT_OPCODE:
			if (scanf("%ld", &ctx->registers[instr.rdest]) != 1) {
				_CT_FAIL();
			}

			break;
		case PRINT_OPCODE:
			printf("%ld\n", ctx->registers[instr.rdest]);
			break;

		default:
			_CT_FAIL();
	}

_CT_EXIT_POINT:
	return ret;
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
			if (ctx->registers[instr.rsrc1] < 0) {
				return S_FAIL;
			}

			ctx->registers[instr.rdest] = 
				(int64_t) sqrt((double)ctx->registers[instr.rsrc1]);
			break;
		case NOT_OPCODE:
			ctx->registers[instr.rdest] = ~(ctx->registers[instr.rsrc1]);
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
		OPERATION_CASE(OR_OPCODE,  |);
		OPERATION_CASE(XOR_OPCODE, ^);
		OPERATION_CASE(AND_OPCODE, &);

		// If we do this without casts, C will do an arithmetic shift
		case SHL_OPCODE:
			ctx->registers[instr.rdest] = 
			(int64_t) ((uint64_t) ctx->registers[instr.rsrc1] << 
					(uint64_t) ctx->registers[instr.rsrc2]);
			break;
		case SHR_OPCODE:
			ctx->registers[instr.rdest] = 
			(int64_t) ((uint64_t) ctx->registers[instr.rsrc1] >>
					(uint64_t) ctx->registers[instr.rsrc2]);
			break;
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
