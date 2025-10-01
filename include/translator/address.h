#ifndef SPU_TRANSLATE_ADDRESS
#define SPU_TRANSLATE_ADDRESS

#include "spu.h"

int parse_address(char *arg, struct spu_addrdata *addr);

int dump_addrdata(struct spu_addrdata addr);

#endif /* SPU_TRANSLATE_ADDRESS  */
