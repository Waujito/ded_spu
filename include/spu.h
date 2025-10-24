/**
 * @file
 *
 * @brief SPU - Software Processing Unit
 */

#ifndef SPU_H
#define SPU_H

#include "spu_asm.h"
#include "pvector.h"

#define RET_STACK_MAX_SIZE (1024)
#define RAM_SIZE (1048576)

#define SCREEN_HEIGHT (15)
#define SCREEN_WIDTH (15)

#define S_HALT (1)

struct spu_context {
	spu_data_t registers[N_REGISTERS];
	spu_data_t RFLAGS;
	spu_instruction_t *instr_buf;
	size_t instr_bufsize;
	size_t ip;
	struct pvector stack;
	struct pvector call_stack;
	int64_t *ram;
	uint64_t screen_height;
	uint64_t screen_width;
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
