#include <assert.h>
#include "spu_asm.h"
#include "spu.h"

static inline int do_conditional_jump(
	struct spu_context *ctx, int condition) {
	assert (ctx);

	switch (condition) {
		case UNCONDITIONAL_JMP:
			break;
		case EQUALS_JMP:
			if (!(ctx->RFLAGS & CMP_EQ_FLAG)) {
				return 0;
			}
			break;
		case NOT_EQUALS_JMP:
			if ((ctx->RFLAGS & CMP_EQ_FLAG)) {
				return 0;
			}
			break;
		case GREATER_EQUALS_JMP:
			if ((ctx->RFLAGS & CMP_SIGN_FLAG)) {
				return 0;
			}
			break;
		case GREATER_JMP:
			if ((ctx->RFLAGS & CMP_SIGN_FLAG) ||
			    (ctx->RFLAGS & CMP_EQ_FLAG)) {
				return 0;
			}
			break;
		case LESS_EQUALS_JMP:
			if (!(ctx->RFLAGS & CMP_SIGN_FLAG) &&
			    !(ctx->RFLAGS & CMP_EQ_FLAG)) {
				return 0;
			}
			break;
		case LESS_JMP:
			if (!(ctx->RFLAGS & CMP_SIGN_FLAG)) {
				return 0;
			}
			break;
		default:
			return -1;
	}

	return 1;
}


OP_EXEC_FN(jmp_exec) {
	int64_t new_ip = (int64_t)ctx->ip + instr.jmp_position;

	int status = do_conditional_jump(ctx, instr.jmp_condition);
	if (status < 0) {
		return S_FAIL;
	} else if (status > 0) {
		if (	new_ip < 0 ||
			(size_t)new_ip > ctx->instr_bufsize) {
			return S_FAIL;
		}

		ctx->ip = (size_t)new_ip;
	}
	
	return S_OK;
}

OP_EXEC_FN(call_exec) {
	int ret = S_OK;

	if (ctx->call_stack.len >= RET_STACK_MAX_SIZE) {
		log_error("call stack overflow");
		_CT_FAIL();
	}


	_CT_FAIL_NONZERO(
		pvector_push_back(&ctx->call_stack, &ctx->ip));

	_CT_CHECKED(jmp_exec(ctx, instr));

_CT_EXIT_POINT:
	return ret;
}

OP_EXEC_FN(ret_exec) {
	int ret = S_OK;

	uint64_t old_ip = 0;
	_CT_FAIL_NONZERO(
		pvector_pop_back(&ctx->call_stack, &old_ip));
	
	if (old_ip > ctx->instr_bufsize) {
		_CT_FAIL();
	}

	ctx->ip = old_ip;

_CT_EXIT_POINT:
	return ret;
}
