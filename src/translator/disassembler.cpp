/**
 * @file
 *
 * @brief Disassembler runner
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "spu_bit_ops.h"

#include "spu.h"

static int disasm_instruction(struct spu_instruction *instr) {
	assert (instr);

	uint32_t opcode = instr->opcode.code;
	int ret = S_OK;
	const struct op_cmd *op_cmd = NULL;

	int is_directive = 0;
	struct spu_instr_data instr_data = {0};

	if (opcode == DIRECTIVE_OPCODE) {
		is_directive = 1;

		_CT_CHECKED(get_directive_opcode(&opcode, instr));
	}

	op_cmd = find_op_cmd_opcode(opcode, is_directive);
	if (!op_cmd) {
		log_error("opcode <%u> not found", opcode);
		_CT_FAIL();
	}

	_CT_CHECKED(op_cmd->layout->parse_bin_fn(instr, &instr_data));
	_CT_CHECKED(op_cmd->layout->write_asm_fn(&instr_data, stdout));
	fprintf(stdout, "\n");

_CT_EXIT_POINT:
	return ret;
}

static int DisasmLoop(struct spu_context *ctx) {
	assert (ctx);

	while (ctx->ip < ctx->instr_bufsize) {
		struct spu_instruction instr = {
			.instruction = ctx->instr_buf[ctx->ip]
		};
		ctx->ip++;

		if (disasm_instruction(&instr)) {
			return S_FAIL;
		}
	}

	return S_OK;
}

static int disasm(const char *in_filename) {
	SPUCreate(ctx);

	int ret = S_OK;

	_CT_CHECKED(SPULoadBinary(&ctx, in_filename)); // FIXME snake pascal

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
		log_error("Disassembling failure");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
