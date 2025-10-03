#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>

#include "types.h"
#include "spu.h"
#include "ctio.h"
#include "spu_debug.h"

struct spu_context {
	uint64_t registers[N_REGISTERS];
	spu_instruction_t *instr_buf;
	size_t instr_bufsize;
	size_t ip; 
};

int dump_spu(struct spu_context *ctx, FILE *out_stream) {
	assert (ctx);

#define DPRINT(...) fprintf(out_stream, __VA_ARGS__)

	DPRINT(	"SPU Core Dumped: \n" );
	DPRINT("r0: <");
	buf_dump_hex(&ctx->registers[0], sizeof(ctx->registers[0]), out_stream);
	DPRINT(	">\n");
	DPRINT("r1: <");
	buf_dump_hex(&ctx->registers[1], sizeof(ctx->registers[0]), out_stream);
	DPRINT(	">\n");
	DPRINT("rsp: <");
	buf_dump_hex(&ctx->registers[REGISTER_RSP_CODE], 
			sizeof(ctx->registers[0]), out_stream);
	DPRINT(	">\n");
	DPRINT("ip: <%lx>\n", ctx->ip);

#undef DPRINT

	return S_OK;
}

int execute(struct spu_context *ctx) {
	assert (ctx);

	while (ctx->ip < ctx->instr_bufsize) {
		struct spu_instruction instr = {
			.instruction = ctx->instr_buf[ctx->ip++]
		};


		switch (instr.opcode.code) {
			case MOV_OPCODE:
				printf("Mov instruction called\n");
				break;
			case LDR_OPCODE:
				printf("LDR instruction called\n");
				break;
			case DUMP_OPCODE:
				printf("OMG\n");
				dump_spu(ctx, stdout);
				break;
			case HALT_OPCODE:
				printf("HALT\n");
				return S_OK;
				break;
			default:
				return S_FAIL;
		}
	}

	return S_OK;
}

int parse_stream(const char *in_filename) {
	spu_instruction_t *instr_buf = NULL;
	size_t instr_bufsize = 0;
	struct spu_context ctx = {0};
	int ret = S_OK;

	_CT_CHECKED(read_file(in_filename, (char **)&instr_buf, &instr_bufsize));

	instr_bufsize /= sizeof(spu_instruction_t);

	ctx = {
		.registers = {0},
		.instr_buf = instr_buf,
		.instr_bufsize = instr_bufsize,
		.ip = 0
	};

	if ((ret = execute(&ctx))) {
		log_error("The program exited with non-zero exit code: <%d>", ret);
		_CT_FAIL();
	}

_CT_EXIT_POINT:
	free(instr_buf);
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

	if (parse_stream(binary_filename)) {
		log_error("The program exited with non-zero exit code");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
