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
typedef uint64_t	spu_data_t;

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
#define LDC_INTEGER_LEN (20)

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

enum spu_cmp_flags {
	CMP_DO_CMP		= 1 << 0,
	CMP_EQ_FLAG		= 1 << 1,
	CMP_SIGN_FLAG		= 1 << 2,
	CMP_OVERFLOW_FLAG	= 1 << 3,
	CMP_RESERVED_FLAG	= 1 << 4,
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

	/// Integer square root
	/// Unary operation
	SQRT_OPCODE	= 0x08,

	/// Single-register operation
	PUSHR_OPCODE	= 0x20,
	/// Single-register operation
	POPR_OPCODE	= 0x21,

	/// Single-register operation
	INPUT_OPCODE	= 0x50,
	/// Single-register operation
	PRINT_OPCODE	= 0x51,

	/// Compares two registers
	CMP_OPCODE	= 0x70,

	/// No-args operation
	DUMP_OPCODE	= 0xFE,
	/// No-args operation
	HALT_OPCODE	= 0xFF,
};

#endif /* SPU_ASM_H */
