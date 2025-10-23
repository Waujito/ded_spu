/**
 * @file
 *
 * @brif BIT operations for SPU
 */

#ifndef SPU_BIT_OPS_H
#define SPU_BIT_OPS_H

#include "spu_asm.h"

#define NO_R_HEAD_BIT	(0)
#define USE_R_HEAD_BIT	(1)

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

static inline int get_directive_opcode(uint32_t *field, const struct spu_instruction *instr) {
	return instr_get_bitfield(field, DIRECTIVE_INSTR_BITLEN, instr, 0);
}
static inline int set_directive_opcode(uint32_t field, struct spu_instruction *instr) {
	if (instr_set_bitfield(field, DIRECTIVE_INSTR_BITLEN, instr, 0) < 0)
		return -1;

	instr->opcode.code = DIRECTIVE_OPCODE;

	return 0;
}

static inline int get_raw_opcode(uint32_t *field, const struct spu_instruction *instr) {
	*field = instr->opcode.code;

	return 0;
}
static inline int set_raw_opcode(uint32_t field, struct spu_instruction *instr) {
	instr->opcode.code = field;

	return 0;
}

int32_t bit_extend_signed(uint32_t unum, size_t num_blen);

int test_integer_bounds(int32_t num, size_t integer_bit_len);

#endif /* SPU_BIT_OPS_H */
