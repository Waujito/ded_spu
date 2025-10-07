/**
 * @file
 *
 * @brif BIT operations for SPU
 */

#ifndef SPU_BIT_OPS_H
#define SPU_BIT_OPS_H

#include "spu_asm.h"


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

static inline int directive_set_bitfield(
	uint32_t field, size_t fieldlen,
	struct spu_instruction *instr, size_t pos) {
	return instr_set_bitfield(field, fieldlen, instr,
			   pos + DIRECTIVE_INSTR_BITLEN);
}

static inline int directive_set_register(spu_register_num_t rn,
			struct spu_instruction *instr, 
			size_t pos, int set_head_bit) {
	return instr_set_register(rn, instr,
			   pos + DIRECTIVE_INSTR_BITLEN, set_head_bit);
}

static inline int directive_get_bitfield(
	uint32_t *field, size_t fieldlen,
	const struct spu_instruction *instr, size_t pos) {
	return instr_get_bitfield(field, fieldlen, instr,
			   pos + DIRECTIVE_INSTR_BITLEN);
}

static inline int directive_get_register(spu_register_num_t *rn,
			const struct spu_instruction *instr,
			size_t pos, int spec_head_bit) {
	return instr_get_register(rn, instr,
			   pos + DIRECTIVE_INSTR_BITLEN, spec_head_bit);
}

#endif /* SPU_BIT_OPS_H */
