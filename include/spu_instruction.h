/**
 * @file
 *
 * @brief Configurable file for SPU and Disasm
 *
 * The file is not marked with guard defines
 * since it should be used directly in cpp files
 * And MUST NEVER duplicate
 *
 * This file is common for disassembler and spu,
 * and should be configured accordingly.
 *
 * Pass SPU_INSTR_MODE_EXEC if you want to use it for execution.
 *	Pass DEBUG_INSTRUCTIONS if you want to enable logging while execution
 *
 * Pass SPU_INSTR_MODE_DISASM if you want to use it for disassembly.
 *
 */

#include <assert.h>
#include <math.h>
#include <string.h>

#include "spu.h"
#include "spu_bit_ops.h"
#include "spu_debug.h"

#ifndef SPU_INSTR_H
#define SPU_INSTR_H
#else
# error "DOUBLE SPU_INSTRUCTION.H INCLUSION FORBIDDEN!"
#endif /* SPU_INSTR_H */

#if ! (defined(SPU_INSTR_MODE_EXEC) || defined(SPU_INSTR_MODE_DISASM))
# error "Configure spu_instruction.h with SPU_INSTR_MODE_EXEC or SPU_INSTR_MODE_DISASM!"
#endif


#define S_EXIT (3)

#define SPU_PRINT_INSTRUCTION_DISASM(stream, instr, ...)			\
	buf_dump_hex(&instr.instruction, sizeof(instr.instruction), stream);	\
	fprintf(stream, "\t");							\
	fprintf(stream, __VA_ARGS__);						\
	fprintf(stream, "\n")


#if defined(SPU_INSTR_MODE_DISASM)
#define INSTR_LOG(...)						\
	SPU_PRINT_INSTRUCTION_DISASM(stdout, __VA_ARGS__);	\
	break
#elif defined(DEBUG_INSTRUCTIONS)
#define INSTR_LOG(...)					\
	eprintf("Executing ");				\
	SPU_PRINT_INSTRUCTION_DISASM(stderr, __VA_ARGS__)
#else
#define INSTR_LOG(...) (void)0
#endif

#ifdef SPU_INSTR_MODE_DISASM
static int DisasmDirectiveInstruction(struct spu_context *ctx,
				      struct spu_instruction instr)
#else /* SPU_DISASM_MODE */
static int SPUExecuteDirective(struct spu_context *ctx, struct spu_instruction instr)
#endif /* SPU_DISASM_MODE */
{
	assert (ctx);
	assert (instr.opcode.code == DIRECTIVE_OPCODE);

	int ret = S_OK;

	uint32_t directive_code = 0;
	spu_register_num_t dest = 0;
	spu_register_num_t src = 0;
	spu_register_num_t src1 = 0;
	spu_register_num_t src2 = 0;

	instr_get_bitfield(&directive_code, 10, &instr, 0);

	switch (directive_code) {

#define TRIPLE_REG_READ__(instr_name)							\
	_CT_CHECKED(directive_get_register(&dest, &instr,				\
				0, USE_R_HEAD_BIT));					\
	_CT_CHECKED(directive_get_register(&src1, &instr,				\
				FREGISTER_BIT_LEN, NO_R_HEAD_BIT));			\
	_CT_CHECKED(directive_get_register(&src2, &instr,				\
				FREGISTER_BIT_LEN + REGISTER_BIT_LEN, NO_R_HEAD_BIT));	\
	INSTR_LOG(instr, instr_name " r%d r%d r%d", dest, src1, src2)

		case ADD_OPCODE:
			TRIPLE_REG_READ__("add");
			ctx->registers[dest] = ctx->registers[src1] + ctx->registers[src2];
			break;
		case MUL_OPCODE:
			TRIPLE_REG_READ__("mul");
			ctx->registers[dest] = ctx->registers[src1] * ctx->registers[src2];
			break;
		case SUB_OPCODE:
			TRIPLE_REG_READ__("sub");
			ctx->registers[dest] = ctx->registers[src1] - ctx->registers[src2];
			break;
		case DIV_OPCODE:
			TRIPLE_REG_READ__("div");

			if (!ctx->registers[src2]) {
				_CT_FAIL();
			}
			ctx->registers[dest] = ctx->registers[src1] / ctx->registers[src2];
			break;
		case MOD_OPCODE:
			TRIPLE_REG_READ__("mod");

			if (!ctx->registers[src2]) {
				_CT_FAIL();
			}
			ctx->registers[dest] = ctx->registers[src1] % ctx->registers[src2];
			break;
#undef TRIPLE_ARG_READ__
		case CMP_OPCODE:
			_CT_CHECKED(directive_get_register(&dest, &instr,
						0, USE_R_HEAD_BIT));
			_CT_CHECKED(directive_get_register(&src, &instr,
						FREGISTER_BIT_LEN, NO_R_HEAD_BIT));

			INSTR_LOG(instr, "cmp r%d r%d", dest, src);

			{
				int64_t lnum = (int64_t)ctx->registers[dest];
				int64_t rnum = (int64_t)ctx->registers[src];

				ctx->RFLAGS = 0;

				if (lnum == rnum) {
					ctx->RFLAGS |= CMP_EQ_FLAG;
				}
				if (lnum < rnum) {
					ctx->RFLAGS |= CMP_SIGN_FLAG;
				}
			}

			break;

		case SQRT_OPCODE:
			_CT_CHECKED(directive_get_register(&dest, &instr,
						0, USE_R_HEAD_BIT));
			_CT_CHECKED(directive_get_register(&src, &instr,
						FREGISTER_BIT_LEN, NO_R_HEAD_BIT));

			INSTR_LOG(instr, "sqrt r%d r%d", dest, src);

			{
				int64_t snum = ctx->registers[src];

				if (snum < 0) {
					_CT_FAIL();
				}

				double d = (double)snum;
				d = sqrt(d);
				snum = (int64_t)d;
				ctx->registers[dest] = snum;
			}

			break;

		case PUSHR_OPCODE:
			_CT_CHECKED(directive_get_register(&dest, &instr,
							0, USE_R_HEAD_BIT));

			INSTR_LOG(instr, "pushr r%d", dest);

			if (pvector_push_back(&ctx->stack, &(ctx->registers[dest])))
				_CT_FAIL();

			break;
		case POPR_OPCODE:
			_CT_CHECKED(directive_get_register(&dest, &instr,
							0, USE_R_HEAD_BIT));

			INSTR_LOG(instr, "popr r%d", dest);

			if (pvector_pop_back(&ctx->stack, &(ctx->registers[dest])))
				_CT_FAIL();
			break;
		case INPUT_OPCODE:
			_CT_CHECKED(directive_get_register(&dest, &instr,
							0, USE_R_HEAD_BIT));

			INSTR_LOG(instr, "input r%d", dest);
			
		{
			if (scanf("%ld", &ctx->registers[dest]) != 1) {
				_CT_FAIL();
			}
		}

			break;
		case PRINT_OPCODE:
			_CT_CHECKED(directive_get_register(&dest, &instr,
							0, USE_R_HEAD_BIT));

			INSTR_LOG(instr, "print r%d", dest);

			printf("%ld\n", ctx->registers[dest]);
			break;

		case DUMP_OPCODE:
			INSTR_LOG(instr, "dump");
			SPUDump(ctx, stdout);
			break;
		case HALT_OPCODE:
			INSTR_LOG(instr, "halt");
			return S_EXIT;
		default:
			INSTR_LOG(instr, "unknown directive: %u",
				directive_code);
			return S_FAIL;
	}

_CT_EXIT_POINT:
	return ret;
}

#ifdef SPU_INSTR_MODE_DISASM
static int DisasmInstruction(struct spu_context *ctx, struct spu_instruction instr)
#else /* SPU_DISASM_MODE */
static int SPUExecuteInstruction(struct spu_context *ctx, struct spu_instruction instr)
#endif /* SPU_DISASM_MODE */

{
	assert (ctx);

	int ret = S_OK;

	uint32_t	unum = 0;
	int32_t		snum = 0;

	spu_register_num_t dest = 0;
	spu_register_num_t src = 0;

	switch (instr.opcode.code) {
		case MOV_OPCODE:
			_CT_CHECKED(instr_get_register(&dest, &instr,
					0, USE_R_HEAD_BIT));
			_CT_CHECKED(instr_get_register(&src, &instr,
					FREGISTER_BIT_LEN + MOV_RESERVED_FIELD_LEN, NO_R_HEAD_BIT));

			INSTR_LOG(instr, "mov r%d r%d", dest, src);

			ctx->registers[dest] = ctx->registers[src];

			break;
		case LDC_OPCODE:
			_CT_CHECKED(instr_get_register(&dest, &instr,
					0, USE_R_HEAD_BIT));
			_CT_CHECKED(instr_get_bitfield(&unum, LDC_INTEGER_BLEN,
					&instr, FREGISTER_BIT_LEN));

			snum = bit_extend_signed(unum, LDC_INTEGER_BLEN);

			INSTR_LOG(instr, "ldc r%d $%d", dest, snum);

			ctx->registers[dest] = snum;

			break;
		case JMP_OPCODE: {
			// !!! Not a register, just flags !!!
			_CT_CHECKED(instr_get_register(&dest, &instr,
				  0, USE_R_HEAD_BIT));
			_CT_CHECKED(instr_get_bitfield(&unum, JMP_INTEGER_BLEN,
					&instr, FREGISTER_BIT_LEN));

			snum = bit_extend_signed(unum, JMP_INTEGER_BLEN);
			int64_t new_ip = (int64_t)ctx->ip + snum;

			INSTR_LOG(instr, "jmp.$%d $%d", dest, snum);

			switch (dest) {
				case UNCONDITIONAL_JMP:
					break;
				case EQUALS_JMP:
					if (!(ctx->RFLAGS & CMP_EQ_FLAG)) {
						goto _CT_EXIT_POINT;
					}
					break;
				case NOT_EQUALS_JMP:
					if ((ctx->RFLAGS & CMP_EQ_FLAG)) {
						goto _CT_EXIT_POINT;
					}
					break;
				case GREATER_EQUALS_JMP:
					if ((ctx->RFLAGS & CMP_SIGN_FLAG)) {
						goto _CT_EXIT_POINT;
					}
					break;
				case GREATER_JMP:
					if ((ctx->RFLAGS & CMP_SIGN_FLAG) ||
					    (ctx->RFLAGS & CMP_EQ_FLAG)) {
						goto _CT_EXIT_POINT;
					}
					break;
				case LESS_EQUALS_JMP:
					if (!(ctx->RFLAGS & CMP_SIGN_FLAG) &&
					    !(ctx->RFLAGS & CMP_EQ_FLAG)) {
						goto _CT_EXIT_POINT;
					}
					break;
				case LESS_JMP:
					if (!(ctx->RFLAGS & CMP_SIGN_FLAG)) {
						goto _CT_EXIT_POINT;
					}
					break;
				default:
					_CT_FAIL();
			}

			if (new_ip < 0 || (size_t)new_ip > ctx->instr_bufsize) {
				_CT_FAIL();
			}

			ctx->ip = (size_t)new_ip;

			break;
		}
		case DIRECTIVE_OPCODE:
#ifdef SPU_INSTR_MODE_DISASM
			return DisasmDirectiveInstruction(ctx, instr);
#else /* SPU_DISASM_MODE */
			return SPUExecuteDirective(ctx, instr);
#endif /* SPU_DISASM_MODE */
		default:
			INSTR_LOG(instr, "unknown %d", instr.opcode.code);
			return S_FAIL;
	}

_CT_EXIT_POINT:
	return ret;
}

