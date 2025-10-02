#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>

#include "types.h"
#include "spu.h"
#include "ctio.h"

struct spu_context {
	uint64_t rax;
	uint8_t *instr_buf;
	size_t instr_bufsize;
	size_t ip; 
};

int buf_dump_hex(void *buf, size_t buflen, FILE *out_stream) {
	assert (buf);
	assert (out_stream);

	for (size_t i = 0; i < buflen; i++) {
		fprintf(out_stream, "%02x", *(((uint8_t *)buf) + i));
	}

	return S_OK;
}

int dump_spu(struct spu_context *ctx, FILE *out_stream) {
	assert (ctx);

#define DPRINT(...) fprintf(out_stream, __VA_ARGS__)

	DPRINT(	"SPU Core Dumped: \n"
		"rax: <");
	buf_dump_hex(&ctx->rax, sizeof(ctx->rax), out_stream);
	DPRINT(	">\n"
		"ip: <%lx>\n", ctx->ip);

#undef DPRINT

	return S_OK;
}

int execute(struct spu_context *ctx) {
	assert (ctx);

	while (ctx->ip < ctx->instr_bufsize) {
		uint8_t opcode = ctx->instr_buf[ctx->ip];

		switch (opcode) {
			case MOV_OPCODE:
			case PUSH_OPCODE:
			case DUMP_OPCODE:
				dump_spu(ctx, stdout);
				break;
			case HALT_OPCODE:
				return S_OK;
				break;
			default:
				return S_FAIL;
		}
		ctx->ip++;
	}

	return S_OK;
}

int parse_stream(const char *in_filename) {
	uint8_t *exec_buf = NULL;
	size_t exec_size = 0;
	
	if (read_file(in_filename, (char **)&exec_buf, &exec_size)) {
		return S_FAIL;
	}

	struct spu_context ctx = {
		.rax = 0,
		.instr_buf = exec_buf,
		.instr_bufsize = exec_size,
		.ip = 0
	};

	return execute(&ctx);
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
		return 1;
	}

	return EXIT_SUCCESS;
}
