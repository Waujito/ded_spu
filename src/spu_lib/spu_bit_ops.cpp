/**
 * @file
 *
 * @brief Bit operations for SPU
 */

#include <assert.h>

#include "spu_asm.h"
#include "spu_bit_ops.h"

uint32_t get_instr_arg(const struct spu_instruction *instr) {
	assert (instr);

	uint32_t arg = instr->arg;
#ifdef __LITTLE_ENDIAN
	arg <<= 8;
#endif
	arg = ntohl(arg);

	return arg;
}

void set_instr_arg(struct spu_instruction *instr, uint32_t arg) {
	assert (instr);

	arg = htonl(arg);

#if __BYTE_ORDER == __LITTLE_ENDIAN
	arg >>= 8;
#endif
	instr->arg = arg;
}

int instr_set_bitfield(
	uint32_t field, size_t fieldlen,
	struct spu_instruction *instr, size_t pos) {
	assert (instr);

	if (pos + fieldlen > SPU_INSTR_ARG_BITLEN) {
		return -1;
	}

	uint32_t mask = (1 << (fieldlen));
	mask -= 1;
	uint32_t arg = get_instr_arg(instr);

	size_t shift = SPU_INSTR_ARG_BITLEN - fieldlen - pos;

	uint32_t shifted_mask = (mask << shift);
	uint32_t shifted_field = (field << shift);

	arg &= (~shifted_mask);
	arg |= (shifted_field);

	set_instr_arg(instr, arg);

	return S_OK;
}

int instr_get_bitfield(
	uint32_t *field, size_t fieldlen,
	const struct spu_instruction *instr, size_t pos) {
	assert (field);
	assert (instr);

	if (pos + fieldlen > SPU_INSTR_ARG_BITLEN) {
		return -1;
	}

	uint32_t mask = (1 << (fieldlen));
	mask -= 1;

	uint32_t arg = get_instr_arg(instr);

	size_t shift = SPU_INSTR_ARG_BITLEN - fieldlen - pos;

	uint32_t shifted_mask = (mask << shift);

	uint32_t shifted_field = (arg & shifted_mask);
	uint32_t real_field = shifted_field >> shift;

	*field = real_field;
	
	return S_OK;
}

int instr_set_register(spu_register_num_t rn,
			struct spu_instruction *instr, 
			size_t pos, int set_head_bit) {
	assert (instr);

	unsigned int use_head_bit = (set_head_bit != 0);

	size_t reg_copy_len = REGISTER_BIT_LEN - use_head_bit;

	if (use_head_bit) {
		instr->opcode.reg_extended = (
			(rn & REGISTER_HIGHBIT_MASK) == REGISTER_HIGHBIT_MASK);
		rn &= (uint8_t)(~REGISTER_HIGHBIT_MASK);
	}

	if (instr_set_bitfield(rn, reg_copy_len, instr, pos)) {
		return S_FAIL;
	}

	return S_OK;
}


int instr_get_register(spu_register_num_t *rn,
			const struct spu_instruction *instr, 
			size_t pos, int spec_head_bit) {
	assert (rn);
	assert (instr);

	unsigned int use_head_bit = (spec_head_bit != 0);

	size_t reg_len = REGISTER_BIT_LEN - use_head_bit;

	uint32_t regnum = 0;
	if (instr_get_bitfield(&regnum, reg_len, instr, pos)) {
		return S_FAIL;
	}

	if (use_head_bit && instr->opcode.reg_extended) {
		regnum |= REGISTER_HIGHBIT_MASK;
	}

	*rn = (uint8_t)regnum;

	return S_OK;
}
