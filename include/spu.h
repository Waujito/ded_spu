#ifndef SPU_H
#define SPU_H

#include "spu_asm.h"
#include "pvector.h"

struct spu_context {
	spu_data_t registers[N_REGISTERS];
	spu_instruction_t *instr_buf;
	size_t instr_bufsize;
	size_t ip;
	pvector stack;
};


int SPUCtor(struct spu_context *ctx);
int SPUDtor(struct spu_context *ctx);

#define SPUCreate(varName)			\
	struct spu_context varName = {{0}};	\
	SPUCtor(&varName);

int SPULoadBinary(struct spu_context *ctx, const char *filename);

int SPUDump(struct spu_context *ctx, FILE *out_stream);

int SPUExecute(struct spu_context *ctx);

#endif /* SPU_H */
