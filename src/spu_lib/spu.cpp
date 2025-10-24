/**
 * @file
 *
 * @brief SPU - Software Processing Unit
 */

#include <stdlib.h>

#include "spu.h"
#include "spu_bit_ops.h"
#include "spu_debug.h"

#ifdef _DEBUG
#define DEBUG_INSTRUCTIONS
#endif

#include "ctio.h"

int SPUCtor(struct spu_context *ctx) {
	assert (ctx);

	*ctx = (struct spu_context) {
		.registers = {0},
		.instr_buf = NULL,
		.instr_bufsize = 0,
		.ip = 0,
		.stack = {0},
		.screen_height = 10,
		.screen_width = 10
	};

	if (pvector_init(&ctx->stack, sizeof(spu_data_t))) {
		return S_FAIL;
	}

	if (pvector_init(&ctx->call_stack, sizeof(uint64_t))) {
		return S_FAIL;
	}

	ctx->ram = malloc(RAM_SIZE);
	if (!ctx->ram) {
		return S_FAIL;
	}

	return S_OK;
}

int SPUDtor(struct spu_context *ctx) {
	assert (ctx);

	free (ctx->instr_buf);
	ctx->instr_buf = NULL;
	ctx->instr_bufsize = 0;

	pvector_destroy(&ctx->stack);
	pvector_destroy(&ctx->call_stack);
	free(ctx->ram);

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

int SPUExecuteInstruction(struct spu_context *ctx, struct spu_instruction instr) {
	assert (ctx);

	uint32_t opcode = instr.opcode.code;
	int ret = S_OK;
	const struct op_cmd *op_cmd = NULL;
	int is_directive = 0;


	if (opcode == DIRECTIVE_OPCODE) {
		is_directive = 1;

		_CT_CHECKED(get_directive_opcode(&opcode, &instr));
	}

	op_cmd = find_op_cmd_opcode(opcode, is_directive);
	if (!op_cmd) {
		_CT_FAIL();
	}

	{
		struct spu_instr_data instr_data = {0};
		_CT_CHECKED(op_cmd->layout->parse_bin_fn(&instr, &instr_data));
#ifdef DEBUG_INSTRUCTIONS
		fprintf(stdout, "Executing <");
		_CT_CHECKED(op_cmd->layout->write_asm_fn(&instr_data, stdout));
		fprintf(stdout, ">\n");
#endif
		
		return op_cmd->exec_fun(ctx, instr_data);
	}

_CT_EXIT_POINT:
	return ret;
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
		if (ret < 0) {
			return S_FAIL;
		} else if (ret > 0) {
			return S_OK;
		}
	}

	return S_OK;
}

// Dumps first n registers
#define N_DUMPED_REGISTERS (6)

int SPUDump(struct spu_context *ctx, FILE *out_stream) {
	assert (ctx);
	assert (out_stream);

#define DPRINT(...) fprintf(out_stream, __VA_ARGS__)

	DPRINT(	"SPU Core Dumped: {\n\n" );
	for (size_t i = 0; i < N_DUMPED_REGISTERS; i++) {
		DPRINT("r%zu:\t<0x", i);
		buf_dump_hex(&ctx->registers[i], sizeof(ctx->registers[0]), out_stream);
		DPRINT(	">\n");
	}

	DPRINT("ip:\t<%lx>\n", ctx->ip);

	DPRINT("\nStack dump:\n");
	pvector_dump(&ctx->stack, out_stream);

	DPRINT("\nCall stack dump:\n");
	pvector_dump(&ctx->call_stack, out_stream);


	DPRINT("\n}\n\n");

#undef DPRINT

	return S_OK;
}
