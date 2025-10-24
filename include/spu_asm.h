/**
 * @file
 *
 * @brief SPU documentation + definitions
 *
 * Instruction layout:
 *
 * 6-bit opcode allows up to 2^6 = 64 different instructions.
 *
 * 64 instructions are not enough, so here will be an extending opcode.
 * So, let's call instructions encoded with this basic opcode, 
 * wide instructions and with extended opcode directive instructions.
 *
 * ```
 * |-----------------------------------------------|---------------------|
 * |                       1 byte                  |        3 bytes      |
 * |--------------|-----------------|--------------|---------------------|
 * | reserved bit | 5th bit of Rd_0 | 6-bit opcode |   instruction args  |
 * |--------------|-----------------|--------------|---------------------|
 * ```
 *
 * Register pointers are called Rd_n, n >= 0
 *
 * All registers are 5-bit fields, which means here are up to 2^5 = 32
 * general-puprose registers.
 *
 * Here is only one named register, RSP.
 * It is reserved right now for the future use
 *
 * The user interface has registers R0-30.
 *
 * In the documentation below, the destination register called Rd.
 *
 * Altough, registers are 5 bit wide. Here is one bit in the 
 * first byte of the instruction which encodes the (typically) first register.
 * So the register is encoded with this flag + 4-bit field
 * in the instruction argument set.
 *
 * All registers are independent from each other.
 * Operations MUST NOT **implicitly** changle the registers (except RSP).
 *
 * The user should pass the instruction all the registers
 * they want to operate on.
 *
 * So, the first register has layout:
 * ```
 * |------------------------------|--------------------------------------|
 * | 1 bit in instruction header  |             4-bit field              |
 * |------------------------------|--------------------------------------|
 * ```
 *
 * Next registers are encoded as:
 * ```
 * |---------------------------------------------------------------------|
 * |                            5-bit field                              |
 * |---------------------------------------------------------------------|
 * ```
 */

#ifndef SPU_ASM_H
#define SPU_ASM_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <bits/endian.h>
#include <arpa/inet.h>

#include "types.h"

/// The SPU will be little-endian
#define SPU_BYTEORDER __LITTLE_ENDIAN

// For internal use
#define MAX_INSTR_ARGS (16)

#define INSTRUCTION_SIZE (4)

#define FREGISTER_BIT_LEN	(4)
#define REGISTER_BIT_LEN	(5)
#define N_REGISTERS		(32)
#define REGISTER_RSP_CODE	(0x1F)
#define REGISTER_RSP_NAME	("rsp")
#define REGISTER_HIGHBIT_MASK	(0x10)
#define REGISTER_4BIT_MASK	(0x0F)
#define REGISTER_5BIT_MASK	(0x1F)

typedef uint32_t	spu_instruction_t;
typedef uint8_t		spu_register_num_t;
typedef int64_t		spu_data_t;

// First character is '.', next goes letters
#define LABEL_MIN_LEN (2)

#define LABEL_MAX_LEN (64)

/**
 * @brief SPU base opcodes
 *
 * The opcode number is 6-bit field up to 2^6 = 64
 *
 * The highest possible number is 0b111111 = 0x3F
 */
enum spu_opcodes {
	/**
	 * @brief Mov instruction
	 *
	 * Moves the value from one register to another.
	 *
	 * Has syntax: mov Rd Rn
	 *
	 * Has layout:
	 * ```
	 * |---------------------------------------------------------------|
	 * |                          3 bytes                              |
	 * |----------|------------------|------------------|--------------|
	 * | 4-bit Rd |  2-bits reserved |   Rn - source    |              |
	 * |----------|------------------|------------------|--------------|
	 * ```
	 */
	MOV_OPCODE	= 0x01,
#define MOV_RESERVED_FIELD_LEN (2)

	/**
	 * @brief Loads the constant to the register.
	 *
	 * Loads the 20-bit integer to the register.
	 *
	 * Has layout:
	 * ```
	 * |---------------------------------------------------------------|
	 * |                          3 bytes                              |
	 * |----------|----------------------------------------------------|
	 * | 4-bit Rd |            20-bit integer number                   |
	 * |----------|----------------------------------------------------|
	 * ```
	 */
	LDC_OPCODE	= 0x02,
#define LDC_INTEGER_BLEN (20)

	/**
	 * @brief Jump instruction
	 *
	 * 5 bits (1 from the instruction header and 4 in arguments)
	 * are reserved for the future use. 
	 *
	 * For example, this instructin may become a conditional instruction too 
	 *
	 * Has layout:
	 * ```
	 * |---------------------------------------------------------------|
	 * |                          3 bytes                              |
	 * |------------------|--------------------------------------------|
	 * |  1 + 4 bit flags |          20-bit integer number             |
	 * |------------------|--------------------------------------------|
	 * ```
	 */
	JMP_OPCODE	= 0x03,
#define JMP_INTEGER_OFF  (4)
#define JMP_INTEGER_BLEN (20)

	CALL_OPCODE	= 0x04,

	/**
	 * @brief Directive operation
	 *
	 * Here are only 64 possible instructions,
	 * but the processor should support a way more.
	 *
	 * That's may be a problem in the future, so here is a
	 * workaround: let 63 instructions accept the wide argument field (of 24 bits)
	 * and add a set of directive instructions, with up to 1024 (10 bit field)
	 * instructions in it. These instruction will support 14-bit field.
	 *
	 * The layout for these instructions:
	 *
	 * ```
	 * |---------------------------------------------------------------|
	 * |                          3 bytes                              |
	 * |---------------------------|-----------------------------------|
	 * | 10-bit instruction opcode |         14-bit argument           |
	 * |---------------------------|-----------------------------------|
	 * ```
	 */
	DIRECTIVE_OPCODE = 0x3e,
#define DIRECTIVE_INSTR_BITLEN (10)
};

enum jmp_conditions {
	UNCONDITIONAL_JMP	= 0x0,
	EQUALS_JMP		= 0x1,
	NOT_EQUALS_JMP		= 0x2,
	GREATER_EQUALS_JMP	= 0x3,
	GREATER_JMP		= 0x4,
	LESS_EQUALS_JMP		= 0x5,
	LESS_JMP		= 0x6,
};

enum spu_cmp_flags {
	CMP_EQ_FLAG		= 1 << 0,
	CMP_SIGN_FLAG		= 1 << 1,
	CMP_OVERFLOW_FLAG	= 1 << 2,
};

#define MAX_BASE_OPCODE (0x3F)
#define SPU_INSTR_ARG_BITLEN (24)

struct spu_baseopcode {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int code:	6;
	unsigned int reg_extended: 1;
	unsigned int reserved1: 1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int reserved1: 1;
	unsigned int reg_extended: 1;
	unsigned int code:	6;
#else
# error	"Please fix <bits/endian.h>"
#endif
} __attribute__((packed));

// TODO No unions, UB
struct spu_instruction {
	union {
		spu_instruction_t instruction;

		struct {
			union {
				uint8_t byte_opcode;
				struct spu_baseopcode opcode;
			};
			uint32_t arg: 24;
		} __attribute__((packed));
	};
};

#define MAX_DIRECTIVE_OPCODE (0x3FF)

#define MAX_OPCODE (MAX_DIRECTIVE_OPCODE)

/**
 * @brief Directive operations
 *
 * Operations are divided by various types:
 *
 * Single register operations:
 * ```
 * |---------------------|
 * |       14 bits       |
 * |----------|----------|
 * | 4-bit Rd |          |
 * |----------|----------|
 * ```
 *
 * Triple-register commands:
 * Rd = op( Rl, Rr )
 * ```
 * |---------------------------------------------|
 * |                 14 bits                     |
 * |----------|------------------|---------------|
 * | 4-bit Rd |     5-bit Rl     |     5-bit Rr  |
 * |----------|------------------|---------------|
 * ```
 *
 * Unary operation commands:
 * Rd = op( Rn )
 * ```
 * |-----------------------------|
 * |            14 bits          |
 * |----------|------------|-----|
 * | 4-bit Rd |  5-bit Rn  |     |
 * |----------|------------|-----|
 * ```
 */
enum spu_directive_opcodes {
	/// Triple-register operation
	ADD_OPCODE	= 0x01,
	/// Triple-register operation
	MUL_OPCODE	= 0x02,
	/// Triple-register operation
	SUB_OPCODE	= 0x03,
	/// Integer division
	/// Triple-register operation
	DIV_OPCODE	= 0x04,
	/// Triple-register operation
	MOD_OPCODE	= 0x05,
	/// Triple-register operation
	SHR_OPCODE	= 0x06,
	/// Triple-register operation
	SHL_OPCODE	= 0x07,

	/// Integer square root
	/// Unary operation
	SQRT_OPCODE	= 0x08,

	OR_OPCODE	= 0x10,
	XOR_OPCODE	= 0x11,
	AND_OPCODE	= 0x12,
	NOT_OPCODE	= 0x13,

	LDM_OPCODE	= 0x16,
	STM_OPCODE	= 0x17,

	/// Single-register operation
	PUSHR_OPCODE	= 0x20,
	/// Single-register operation
	POPR_OPCODE	= 0x21,

	/// Single-register operation
	INPUT_OPCODE	= 0x50,
	/// Single-register operation
	PRINT_OPCODE	= 0x51,


	RET_OPCODE	= 0x60,

	// Returns screen height and width
	SCRHW_OPCODE	= 0x65,
	DRAW_OPCODE	= 0x66,

	/// Compares two registers
	CMP_OPCODE	= 0x70,

	/// No-args operation
	DUMP_OPCODE	= 0xFE,
	/// No-args operation
	HALT_OPCODE	= 0xFF,
};

struct op_layout;

struct spu_instr_data {
	uint32_t opcode;
	const struct op_layout *layout;

	union {
		spu_register_num_t rdest;
		spu_register_num_t jmp_condition;
	};

	spu_register_num_t rsrc1;
	spu_register_num_t rsrc2;

	union {
		uint32_t unum;
		int32_t snum;
		int32_t jmp_position;
	};
};

struct spu_context;
typedef int (*exec_instruction_fn)(struct spu_context *ctx, struct spu_instr_data instr);

struct translating_context;

struct asm_instruction {
	size_t n_args;
	char **argsptrs;
	const struct op_cmd *op_cmd;
	const char *op_arg;

	int is_label;
	int is_empty;
	size_t nline;

	struct translating_context *ctx;
};

typedef int (*parse_binary_fn)(const struct spu_instruction *bin_instr,
			       struct spu_instr_data *instr_data);
typedef int (*write_binary_fn)(const struct spu_instr_data *instr_data,
			       struct spu_instruction *bin_instr);
typedef int (*parse_asm_fn)(const struct asm_instruction *asm_instr,
			       struct spu_instr_data *instr_data);
typedef int (*write_asm_fn)(const struct spu_instr_data *instr_data,
			       FILE *out_stream);

struct op_layout {
	int is_directive;

	parse_binary_fn parse_bin_fn;
	write_binary_fn write_bin_fn;
	parse_asm_fn parse_asm_fn;
	write_asm_fn write_asm_fn;
};

#define DECLARE_OP_LAYOUT(name)				\
extern const struct op_layout opl_##name;

DECLARE_OP_LAYOUT(triple_reg);
DECLARE_OP_LAYOUT(double_reg);
DECLARE_OP_LAYOUT(single_reg);
DECLARE_OP_LAYOUT(noarg);
DECLARE_OP_LAYOUT(ldc);
DECLARE_OP_LAYOUT(mov);
DECLARE_OP_LAYOUT(jmp);

struct op_cmd {
	const char *cmd_name;
	unsigned int opcode;
	const struct op_layout *layout;
	exec_instruction_fn exec_fun;
};

#define OP_EXEC_FN(name)						\
	int name(struct spu_context *ctx, struct spu_instr_data instr)

OP_EXEC_FN(mov_exec);
OP_EXEC_FN(ldc_exec);
OP_EXEC_FN(jmp_exec);
OP_EXEC_FN(call_exec);
OP_EXEC_FN(ret_exec);
OP_EXEC_FN(rpush_pop_exec);
OP_EXEC_FN(simple_io_exec);
OP_EXEC_FN(cmp_exec);
OP_EXEC_FN(arithm_binary_exec);
OP_EXEC_FN(arithm_unary_exec);
OP_EXEC_FN(ram_exec);
OP_EXEC_FN(noarg_exec);
OP_EXEC_FN(scrhw_exec);
OP_EXEC_FN(draw_exec);

#define OP_CMD_ENTRY(cmd_str, cmd_code, cmd_layout, cmd_exec_func)		\
	{.cmd_name = cmd_str, .opcode = cmd_code,				\
	 .layout = cmd_layout, .exec_fun = cmd_exec_func}

static const struct op_cmd op_table[] = {
	OP_CMD_ENTRY("mov",	MOV_OPCODE,	&opl_mov,		arithm_unary_exec),
	OP_CMD_ENTRY("ldc",	LDC_OPCODE,	&opl_ldc,		ldc_exec),
	OP_CMD_ENTRY("jmp",	JMP_OPCODE,	&opl_jmp, 		jmp_exec),
	OP_CMD_ENTRY("call",	CALL_OPCODE,	&opl_jmp,		call_exec),
	OP_CMD_ENTRY("ret",	RET_OPCODE,	&opl_noarg,		ret_exec),
	OP_CMD_ENTRY("pushr",	PUSHR_OPCODE,	&opl_single_reg,	rpush_pop_exec),
	OP_CMD_ENTRY("popr",	POPR_OPCODE,	&opl_single_reg,	rpush_pop_exec),
	OP_CMD_ENTRY("input",	INPUT_OPCODE,	&opl_single_reg,	simple_io_exec),
	OP_CMD_ENTRY("print",	PRINT_OPCODE,	&opl_single_reg,	simple_io_exec),
	OP_CMD_ENTRY("cmp",	CMP_OPCODE,	&opl_double_reg,	cmp_exec),
	OP_CMD_ENTRY("add",	ADD_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("mul",	MUL_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("sub",	SUB_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("div",	DIV_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("mod",	MOD_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("shr",	SHR_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("shl",	SHL_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("or", 	OR_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("xor",	XOR_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("and",	AND_OPCODE,	&opl_triple_reg,	arithm_binary_exec),
	OP_CMD_ENTRY("ldm",	LDM_OPCODE,	&opl_double_reg,	ram_exec),
	OP_CMD_ENTRY("stm",	STM_OPCODE,	&opl_double_reg,	ram_exec),
	OP_CMD_ENTRY("sqrt",	SQRT_OPCODE,	&opl_double_reg,	arithm_unary_exec),
	OP_CMD_ENTRY("not",	NOT_OPCODE,	&opl_double_reg,	arithm_unary_exec),
	OP_CMD_ENTRY("scrhw",	SCRHW_OPCODE,	&opl_double_reg,	scrhw_exec),
	OP_CMD_ENTRY("draw",	DRAW_OPCODE,	&opl_single_reg,	draw_exec),
	OP_CMD_ENTRY("dump",	DUMP_OPCODE,	&opl_noarg,		noarg_exec),
	OP_CMD_ENTRY("halt",	HALT_OPCODE,	&opl_noarg, 		noarg_exec),
	{0}
};

const struct op_cmd *find_op_cmd_opcode(unsigned int opcode, int is_directive);
const struct op_cmd *find_op_cmd(const char *cmd_name);

#endif /* SPU_ASM_H */
