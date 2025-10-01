#ifndef SPU_TRANSLATE_ADDRESS
#define SPU_TRANSLATE_ADDRESS

#include "spu.h"

int parse_address(char *arg, struct spu_addrdata *addr);

int dump_addrdata(struct spu_addrdata addr);

int write_raw_addr(struct spu_addrdata addr, 
		   uint8_t addrbuf[sizeof(struct spu_addrdata)],
		   size_t *addrbuf_len);

#endif /* SPU_TRANSLATE_ADDRESS  */
