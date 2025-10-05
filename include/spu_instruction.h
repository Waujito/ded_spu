#include <assert.h>

#include "spu.h"
#include "spu_bit_ops.h"
#include "spu_debug.h"

/**
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

	uint32_t directive_code = 0;
	instr_get_bitfield(&directive_code, 10, &instr, 0);

	switch (directive_code) {
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

	return S_OK;
}

#ifdef SPU_INSTR_MODE_DISASM
static int DisasmInstruction(struct spu_context *ctx, struct spu_instruction instr)
#else /* SPU_DISASM_MODE */
static int SPUExecuteInstruction(struct spu_context *ctx, struct spu_instruction instr)
#endif /* SPU_DISASM_MODE */

{
	assert (ctx);

	int ret = S_OK;

	uint32_t num = 0;
	spu_register_num_t rd = 0;
	spu_register_num_t rn = 0;
	spu_register_num_t rl = 0;
	spu_register_num_t rr = 0;

	switch (instr.opcode.code) {
		case MOV_OPCODE:
			_CT_CHECKED(instr_get_register(&rd, &instr, 0, 1));
			_CT_CHECKED(instr_get_register(&rn, &instr,
					FREGISTER_BIT_LEN + MOV_RESERVED_FIELD_LEN, 0));

			INSTR_LOG(instr, "mov r%d r%d", rd, rn);

			ctx->registers[rd] = ctx->registers[rn];

			break;
		case LDR_OPCODE:
			_CT_CHECKED(instr_get_register(&rd, &instr, 0, 1));
			_CT_CHECKED(instr_get_bitfield(&num, LDR_INTEGER_LEN,
					&instr, FREGISTER_BIT_LEN));

			INSTR_LOG(instr, "ldr r%d $0x%x", rd, num);

			num = htole32(num);
			ctx->registers[rd] = num;

			break;
		case ADD_OPCODE:
			_CT_CHECKED(instr_get_register(&rd, &instr, 0, 1));
			_CT_CHECKED(instr_get_register(&rl, &instr,
				  FREGISTER_BIT_LEN, 0));
			_CT_CHECKED(instr_get_register(&rr, &instr,
				  FREGISTER_BIT_LEN + REGISTER_BIT_LEN, 0));

			INSTR_LOG(instr, "add r%d r%d r%d", rd, rl, rr);

			ctx->registers[rd] = ctx->registers[rl] + ctx->registers[rr];

			break;
		case MUL_OPCODE:
			_CT_CHECKED(instr_get_register(&rd, &instr, 0, 1));
			_CT_CHECKED(instr_get_register(&rl, &instr,
				  FREGISTER_BIT_LEN, 0));
			_CT_CHECKED(instr_get_register(&rr, &instr,
				  FREGISTER_BIT_LEN + REGISTER_BIT_LEN, 0));

			INSTR_LOG(instr, "mul r%d r%d r%d", rd, rl, rr);

			ctx->registers[rd] = ctx->registers[rl] * ctx->registers[rr];

			break;
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

