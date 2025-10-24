#ifndef OPLS_COMMON_H
#define OPLS_COMMON_H

#include "spu_asm.h"

static const struct op_cmd *opcode_layout_search(unsigned int opcode,
						 const struct op_layout *opl) {
	
	const struct op_cmd *op_cmd_ptr = op_table;

	while (op_cmd_ptr->cmd_name != NULL) {
		if (	op_cmd_ptr->opcode == opcode &&
			op_cmd_ptr->layout == opl) {
			return op_cmd_ptr;
		}

		op_cmd_ptr++;
	}

	return NULL;
}

#define DEFINE_BINARY_PARSER(name, ...)						\
static int opl_##name##_parse_binary(const struct spu_instruction *bin_instr,	\
			    struct spu_instr_data *instr_data) {		\
	assert(bin_instr);							\
	assert(instr_data);							\
										\
	int ret = S_OK;								\
										\
	__VA_ARGS__								\
										\
	instr_data->layout = &opl_##name;					\
_CT_EXIT_POINT:									\
	return ret;								\
}										\


#define DEFINE_BINARY_WRITER(name, ...)						\
static int opl_##name##_write_binary(const struct spu_instr_data *instr_data,	\
			      struct spu_instruction *bin_instr) {		\
	assert(instr_data);							\
	assert(instr_data->layout == &opl_##name);				\
	assert(bin_instr);							\
										\
	int ret = S_OK;								\
										\
	__VA_ARGS__								\
										\
_CT_EXIT_POINT:									\
	return ret;								\
}										\


#define DEFINE_ASM_PARSER(name, ...)						\
static int opl_##name##_parse_asm(const struct asm_instruction *asm_instr,	\
			   struct spu_instr_data *instr_data) {			\
	assert(asm_instr);							\
	assert(asm_instr->op_cmd->layout == &opl_##name);			\
	assert(instr_data);							\
										\
	int ret = S_OK;								\
										\
	__VA_ARGS__								\
										\
	instr_data->layout = &opl_##name;					\
	instr_data->opcode = asm_instr->op_cmd->opcode;				\
										\
_CT_EXIT_POINT:									\
	return ret;								\
}										\

#define DEFINE_ASM_WRITER(name, ...)						\
static int opl_##name##_write_asm(const struct spu_instr_data *instr_data,	\
		     FILE *out_stream) {					\
	assert (instr_data);							\
	assert (instr_data->layout == &opl_##name);				\
	assert (out_stream);							\
										\
	int ret = S_OK;								\
	int status = 0;								\
										\
	const struct op_cmd *op_cmd = opcode_layout_search(			\
			instr_data->opcode, &opl_##name);			\
	if (!op_cmd) {								\
		assert(0 && "op_cmd not found");				\
		_CT_FAIL();							\
	}									\
										\
	__VA_ARGS__								\
										\
	if (status < 0) {							\
		_CT_FAIL();							\
	}									\
										\
_CT_EXIT_POINT:									\
	return ret;								\
}										\

#define DEFINE_BINASM_PARSERS(name, is_directive)				\
const struct op_layout opl_##name = {						\
	is_directive,								\
	opl_##name##_parse_binary,						\
	opl_##name##_write_binary,						\
	opl_##name##_parse_asm,							\
	opl_##name##_write_asm							\
}

#endif /* OPLS_COMMON_H */
