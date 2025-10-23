#ifndef OPLS_COMMON_H
#define OPLS_COMMON_H

#include "spu_asm.h"

typedef int (*parse_binary_fn)(const struct spu_instruction *bin_instr,
			       struct spu_instr_data *instr_data);
typedef int (*write_binary_fn)(const struct spu_instr_data *instr_data,
			       struct spu_instruction *bin_instr);
typedef int (*parse_asm_fn)(const struct asm_instruction *asm_instr,
			       struct spu_instr_data *instr_data);
typedef int (*write_asm_fn)(const struct spu_instr_data *instr_data,
			       FILE *out_stream);

#define DECLARE_BINASM_PARSERS(name)						\
int name##_parse_binary(const struct spu_instruction *bin_instr,		\
			     struct spu_instr_data *instr_data);		\
										\
int name##_write_binary(const struct spu_instr_data *instr_data,		\
			     struct spu_instruction *bin_instr);		\
										\
int name##_parse_asm(const struct asm_instruction *asm_instr,			\
			  struct spu_instr_data *instr_data);			\
										\
int name##_write_asm(const struct spu_instr_data *instr_data,			\
			  FILE *out_stream)					\


#define DEFINE_BINARY_PARSER(name, opl, ...)					\
int name##_parse_binary(const struct spu_instruction *bin_instr,		\
			    struct spu_instr_data *instr_data) {		\
	assert(bin_instr);							\
	assert(instr_data);							\
										\
	int ret = S_OK;								\
										\
	__VA_ARGS__								\
										\
	instr_data->layout = opl;						\
_CT_EXIT_POINT:									\
	return ret;								\
}										\


#define DEFINE_BINARY_WRITER(name, opl, ...)					\
int name##_write_binary(const struct spu_instr_data *instr_data,		\
			      struct spu_instruction *bin_instr) {		\
	assert(instr_data);							\
	assert(instr_data->layout == opl);					\
	assert(bin_instr);							\
										\
	int ret = S_OK;								\
										\
	__VA_ARGS__								\
										\
_CT_EXIT_POINT:									\
	return ret;								\
}										\


#define DEFINE_ASM_PARSER(name, opl, ...)					\
int name##_parse_asm(const struct asm_instruction *asm_instr,			\
			   struct spu_instr_data *instr_data) {			\
	assert(asm_instr);							\
	assert(asm_instr->op_cmd->layout == opl);				\
	assert(instr_data);							\
										\
	int ret = S_OK;								\
										\
	__VA_ARGS__								\
										\
	instr_data->layout = opl;						\
	instr_data->opcode = asm_instr->op_cmd->opcode;				\
										\
_CT_EXIT_POINT:									\
	return ret;								\
}										\


#define DEFINE_ASM_WRITER(name, opl, ...)					\
int name##_write_asm(const struct spu_instr_data *instr_data,			\
		     FILE *out_stream) {					\
	assert (instr_data);							\
	assert (instr_data->layout == opl);					\
	assert (out_stream);							\
										\
	int ret = S_OK;								\
	int status = 0;								\
										\
	const struct op_cmd *op_cmd = opcode_layout_search(			\
				instr_data->opcode, opl);			\
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


static const struct op_cmd *opcode_layout_search(unsigned int opcode,
						 enum instruction_layout layout) {
	
	const struct op_cmd *op_cmd_ptr = op_table;

	while (op_cmd_ptr->cmd_name != NULL) {
		if (	op_cmd_ptr->opcode == opcode &&
			op_cmd_ptr->layout == layout) {
			return op_cmd_ptr;
		}

		op_cmd_ptr++;
	}

	return NULL;
}

DECLARE_BINASM_PARSERS(itriple_reg);
DECLARE_BINASM_PARSERS(idouble_reg);
DECLARE_BINASM_PARSERS(isingle_reg);
DECLARE_BINASM_PARSERS(inoarg);
DECLARE_BINASM_PARSERS(ildc);
DECLARE_BINASM_PARSERS(imov);


struct op_layout {
	uint32_t layout;

	parse_binary_fn parse_bin_fn;
	write_binary_fn write_bin_fn;
	parse_asm_fn parse_asm_fn;
	write_asm_fn write_asm_fn;
};

static const struct op_layout op_layout_table[] = {
#define OP_LAYOUT_ENTRY(opl, fn_prefix)					\
	{opl, fn_prefix##_parse_binary, fn_prefix##_write_binary,	\
		fn_prefix##_parse_asm, fn_prefix##_write_asm}		\

	OP_LAYOUT_ENTRY(OPL_NOARG,	inoarg),
	OP_LAYOUT_ENTRY(OPL_SINGLE_REG, isingle_reg),
	OP_LAYOUT_ENTRY(OPL_DOUBLE_REG, idouble_reg),
	OP_LAYOUT_ENTRY(OPL_TRIPLE_REG, itriple_reg),
	OP_LAYOUT_ENTRY(OPL_LDC, ildc),
	OP_LAYOUT_ENTRY(OPL_MOV, imov),

	{0}

#undef OP_LAYOUT_ENTRY
};

static inline const struct op_layout *find_op_layout(uint32_t layout) {
	const struct op_layout *op_lt_ptr = op_layout_table;

	while (op_lt_ptr->parse_asm_fn != NULL) {
		if (op_lt_ptr->layout == layout) {
			return op_lt_ptr;
		}
		op_lt_ptr++;
	}

	return NULL;
}

#endif /* OPLS_COMMON_H */
