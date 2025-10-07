#ifndef SPU_ASM_H
#define SPU_ASM_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <bits/endian.h>
#include <arpa/inet.h>

#include "types.h"

/**
 * The SPU will be little-endian
 */
#define SPU_BYTEORDER __LITTLE_ENDIAN

// For internal use
#define MAX_INSTR_ARGS (16)

#define INSTRUCTION_SIZE (4)

/**
 * Instruction layout:
 * 6-bit opcode allows up to 2^6 = 64 different instructions with
 * possibility of extension the instruction set
 *
 * |-----------------------------------------------|---------------------|
 * |                       1 byte                  |        3 bytes      |
 * |--------------|-----------------|--------------|---------------------|
 * | reserved bit | 5th bit of rd_0 | 6-bit opcode | instruction payload |
 * |--------------|-----------------|--------------|---------------------|
 *
 * Register pointers are called rd_n. n >= 0
 * All registers are 5-bit fields which means here are up to 2^5 = 32
 * general registers.
 * 
 *
 * Here is only one named register, RSP.
 * The user interface has registers R0-30 + (RSP = 0x1F).
 *
 * In the documentation below, the destination register called Rd.
 *
 * Rds are typically encoded with 1 bit in header and 4 bits in
 * instruction payload.
 *
 * All registers are independent from each other.
 * Operations MUST NOT implicitly changle the registers (except RSP).
 *
 * The user should pass to the instruction all the registers
 * he wants to operate on.
 *
 * rd_0 register layout:
 * |------------------------------|--------------------------------------|
 * | 1 bit in instruction header  |             4-bit field              |
 * |------------------------------|--------------------------------------|
 *
 * rd_n for n > 0 register layout:
 * |---------------------------------------------------------------------|
 * |                            5-bit field                              |
 * |---------------------------------------------------------------------|
 *
 */

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

// The opcode number is 6-bit field up to 2^6 = 64
// The highest possible number is 0b111111 = 0x3F
enum spu_opcodes {
	/**
	 * Mov instruction.
	 * Moves the value from one register to another.
	 *
	 * Has syntax: mov rd rn
	 *
	 * Has layout:
	 * |---------------------------------------------------------------|
	 * |                          3 bytes                              |
	 * |----------|------------------|---------------------------------|
	 * | 4-bit Rd |  2-bits reserved | Rn - source  |                  |
	 * |----------|------------------|---------------------------------|
	 */
#define MOV_RESERVED_FIELD_LEN (2)
	MOV_OPCODE	= (0x01),

	/**
	 * Loads the constant to the register.
	 * Loads the 20-bit integer to the register.
	 *
	 * Has layout:
	 * |---------------------------------------------------------------|
	 * |                          3 bytes                              |
	 * |----------|----------------------------------------------------|
	 * | 4-bit Rd |            20-bit integer number                   |
	 * |----------|----------------------------------------------------|
	 */
#define LDC_INTEGER_LEN (20)
	LDC_OPCODE	= (0x02),	

	/**
	 * Jump instructions
	 *
	 * Has layout:
	 * |---------------------------------------------------------------|
	 * |                          3 bytes                              |
	 * |------------------|--------------------------------------------|
	 * |  1 + 4 bit flags |          20-bit integer number             |
	 * |------------------|--------------------------------------------|
	 */
#define JMP_INTEGER_OFF  (4)
#define JMP_INTEGER_BLEN (20)
	JMP_OPCODE	= (0x03),

	/**
	 * Here are only 64 possible instructions,
	 * but the processor should support a way more.
	 *
	 * That's may be a problem in the future, so here is a
	 * workaround: let 63 instructions accept wide argument field (of 24 bits)
	 * and add a set of directive instructions, with up to 1024 (10 bit field)
	 * instructions in it. The layout for these instructions:
	 *
	 * |---------------------------------------------------------------|
	 * |                          3 bytes                              |
	 * |---------------------------|-----------------------------------|
	 * | 10-bit instruction opcode |         14-bit argument           |
	 * |---------------------------|-----------------------------------|
	 *
	 */
#define DIRECTIVE_INSTR_BITLEN (10)
	DIRECTIVE_OPCODE= (0x3e),
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
enum spu_directive_opcodes {
	/**
	 * Triple-register commands goes below. The syntax:
	 * rd = op( rl, rr )
	 *
	 * Has layout:
	 * |---------------------------------------------|
	 * |                 14 bits                     |
	 * |----------|------------------|---------------|
	 * | 4-bit Rd |     5-bit rl     |     5-bit rr  |
	 * |----------|------------------|---------------|
	 */
	ADD_OPCODE	= (0x01),
	MUL_OPCODE	= (0x02),
	SUB_OPCODE	= (0x03),
	// Integer division
	DIV_OPCODE	= (0x04),
	MOD_OPCODE	= (0x05),

	/**
	 * Unary operations on one register.
	 * Accepts two registers: rd = op( rn )
	 *
	 * Has layout:
	 * |-----------------------------|
	 * |            14 bits          |
	 * |----------|------------|-----|
	 * | 4-bit Rd |  5-bit Rn  |     |
	 * |----------|------------|-----|
	 *
	 */
	// Integer square root
	SQRT_OPCODE	= (0x08),

	/**
	 * Single register operations.
	 *
	 * Has layout:
	 * |---------------------|
	 * |       14 bits       |
	 * |----------|----------|
	 * | 4-bit Rd |          |
	 * |----------|----------|
	 *
	 */
	PUSHR_OPCODE	= (0x20),
	POPR_OPCODE	= (0x21),

	DUMP_OPCODE	= (0xFE),
	HALT_OPCODE	= (0xFF),
};

#endif /* SPU_ASM_H */
