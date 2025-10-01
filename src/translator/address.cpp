#include <assert.h>
#include <string.h>
#include <stdlib.h>


#include "types.h"
#include "spu.h"
#include "translator/address.h"

const static struct reg_table {
	const char *reg_name;
	unsigned int reg_src_number;
	int is_extension;
	enum spu_datalengths reg_len;
} reg_table[] = {
	{"rax", SPU_DSRC_RAX, 0, SPU_DLEN_8BYTE},
	{"rsp", SPU_DSRC_RSP, 0, SPU_DLEN_8BYTE},
	{"rcx", SPU_DSRC_RCX, 0, SPU_DLEN_8BYTE},
	{"rbp", SPU_DSRC_RBP, 0, SPU_DLEN_8BYTE},
	{"rdx", SPU_DSRC_RDX, 0, SPU_DLEN_8BYTE},
	{"rsi", SPU_DSRC_RSI, 0, SPU_DLEN_8BYTE},
	{"rdi", SPU_DSRC_RDI, 0, SPU_DLEN_8BYTE},
	{0}
};

static int register_lookup(const char *str, size_t strlen,
			struct reg_table *rgt) {
	assert (str);
	assert (rgt);


	const struct reg_table *reg_ptr = reg_table;
	while (reg_ptr->reg_name != NULL) {
		if (!strncmp(str, reg_ptr->reg_name, strlen)) {
			*rgt = *reg_ptr;
			return S_OK;
		}
		reg_ptr++;
	}

	return S_FAIL;
}

int parse_address(char *arg,
			 struct spu_addrdata *addr) {
	assert (arg);
	assert (addr);

	size_t arglen = strlen(arg);

	if (arglen == 0) {
		return S_FAIL;
	}

	int deref = 0;


	// Dereference has syntax *(%rax)
	if (arg[0] == '*') {
		if (	arglen <= 3 || 
			arg[1] != '(' || 
			arg[arglen - 1] != ')') {

			return S_FAIL;
		}

		arg[arglen - 1] = '\0';

		arg += 2;
		// *( and )
		arglen -= 3;
		deref = 1;
	}

	if (arglen == 0) {
		return S_FAIL;
	}

	// Parse registers
	if (arg[0] == '%') {
		arg++;
		arglen--;
		struct spu_addrhead addrhead = {0};
		struct reg_table rgt = {0};

		if (register_lookup(arg, arglen, &rgt)) {
			return S_FAIL;
		}

		addrhead.deref = deref > 0;
		if (!rgt.is_extension) {
			addrhead.source = rgt.reg_src_number;
		} else {
			log_error("Sorry, not implemented");
			return S_FAIL;
		}

		addrhead.datalength = rgt.reg_len;
		memcpy(addr, &addrhead, sizeof(addrhead));

		addr->head = addrhead;

		return S_OK;
	} else if (arg[0] == '$') {
		arg++;
		arglen--;

		char *endptr = NULL;
		long long number = strtoll(arg, &endptr, 0);
		if (!(*endptr == '\0' && *arg != '\0')) {
			return S_FAIL;
		}

		uint64_t raw_number = 0;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		memcpy(&raw_number, &number, sizeof(number));

#elif __BYTE_ORDER == __BIG_ENDIAN
	memcpy(	(char *)(&raw_number) +
			(sizeof(raw_number) - sizeof(number)),
			&number, sizeof(number));
#else
# error	"Please fix <bits/endian.h>"
#endif
		
		raw_number = htole64(raw_number);

		struct spu_addrhead addrhead = {0};
		addrhead.source = SPU_DSRC_DIRECT;
		addrhead.datalength = SPU_DLEN_8BYTE;
		addrhead.deref = deref > 0;

		addr->head = addrhead;
		addr->direct_number = raw_number;

		return S_OK;
	}

	return S_FAIL;
}

int dump_addrdata(struct spu_addrdata addr) {
	printf("addrhead: <0x%02x>;", *(unsigned char*)&addr.head);
	if (addr.head.source == SPU_DSRC_DIRECT) {
		printf(" number: <0x");
		uint64_t number = addr.direct_number;
		uint8_t *num_iter = (uint8_t *)&number;

		for (size_t i = 0; i < (1 << addr.head.datalength); i++) {
			
			printf("%02x", num_iter[i]);
		}
		printf(">");
	}

	printf("\n");

	return 0;
}
