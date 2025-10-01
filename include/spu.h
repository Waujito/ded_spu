#ifndef SPU_H
#define SPU_H

#include <netinet/ip.h>
#include <bits/endian.h>

#define SPU_BYTEORDER __LITTLE_ENDIAN

#define MAX_OPCODE_ARGSLEN (16)

// Highest bit should always be reserved
// for variable-length commands
enum spu_opcodes {
	MOV_OPCODE	= (0x01),
	PUSH_OPCODE	= (0x02),
	HALT_OPCODE	= (0x73),
	DUMP_OPCODE	= (0x7d),
};

// 2-bit field => max is 0b11 = 0x03
enum spu_datalengths {
	SPU_DLEN_1BYTE	= (0x00),
	SPU_DLEN_2BYTE	= (0x01),
	SPU_DLEN_4BYTE	= (0x02),
	SPU_DLEN_8BYTE	= (0x03)
};


// 4-bit field => max is 0b1111 = 0x0F
enum spu_addrsources {
	SPU_DSRC_RAX	= (0x01),
	SPU_DSRC_RSP	= (0x02),
	SPU_DSRC_RCX	= (0x03),
	SPU_DSRC_RBP	= (0x04),
	SPU_DSRC_RDX	= (0x05),
	SPU_DSRC_RSI	= (0x06),
	SPU_DSRC_RBX	= (0x07),
	SPU_DSRC_RDI	= (0x08),

	SPU_DSRC_DIRECT	= (0x0a),
// Indicates the datasource is extended in the next byte
	SPU_DSRC_EXTEND	= (0x0e),
};

// 1-byte address header
struct spu_addrhead {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int datalength: 2;
	unsigned int source: 4;
	unsigned int deref: 1;
	unsigned int reserved:	1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int reserved:	1;
	unsigned int deref: 1;
	unsigned int source: 4;
	unsigned int datalength: 2;
#else
# error	"Please fix <bits/endian.h>"
#endif
} __attribute__((packed));

struct spu_addrdata {
	struct spu_addrhead head;

	// MUST be little-endian
	uint64_t direct_number;
};

#endif /* SPU_H */
