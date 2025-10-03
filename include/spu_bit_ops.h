#ifndef SPU_BIT_OPS_H
#define SPU_BIT_OPS_H

#include "spu.h"


uint32_t get_instr_arg(const struct spu_instruction *instr);
void set_instr_arg(struct spu_instruction *instr, uint32_t arg);

int instr_set_bitfield(
	uint32_t field, size_t fieldlen,
	struct spu_instruction *instr, size_t pos);

int instr_set_register(spu_register_num_t rn,
			struct spu_instruction *instr, 
			size_t pos, int set_head_bit);

int instr_get_bitfield(
	uint32_t *field, size_t fieldlen,
	const struct spu_instruction *instr, size_t pos);

int instr_get_register(spu_register_num_t *rn,
			const struct spu_instruction *instr, 
			size_t pos, int spec_head_bit);

#endif /* SPU_BIT_OPS_H */
