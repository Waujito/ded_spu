#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"
#include "spu.h"
#include "translator/address.h"

#define REGTABLE_ARG_EXTENSION (1 << 0)

const static struct reg_table {
	const char *reg_name;
	unsigned int reg_src_number;
	int flags;
	enum spu_datalengths reg_len;
} reg_table[] = {
	{"rax", SPU_DSRC_AX, 0, SPU_DLEN_8BYTE},
	{"rsp", SPU_DSRC_SP, 0, SPU_DLEN_8BYTE},
	{"rcx", SPU_DSRC_CX, 0, SPU_DLEN_8BYTE},
	{"rbp", SPU_DSRC_BP, 0, SPU_DLEN_8BYTE},
	{"rdx", SPU_DSRC_DX, 0, SPU_DLEN_8BYTE},
	{"rsi", SPU_DSRC_SI, 0, SPU_DLEN_8BYTE},
	{"rdi", SPU_DSRC_DI, 0, SPU_DLEN_8BYTE},
	{0}
};

static int register_lookup(const char *str, size_t strlen,
			const struct reg_table **rgt) {
	assert (str);
	assert (rgt);


	const struct reg_table *reg_ptr = reg_table;
	while (reg_ptr->reg_name != NULL) {
		if (!strncmp(str, reg_ptr->reg_name, strlen)) {
			*rgt = reg_ptr;
			return S_OK;
		}
		reg_ptr++;
	}

	return S_FAIL;
}

static int addr_to_reg_lookup(const struct spu_addrdata *addr,
				const struct reg_table **rgt) {
	assert (addr);
	assert (rgt);

	const struct reg_table *reg_ptr = reg_table;
	while (reg_ptr->reg_name != NULL) {
		if (addr->head.source == reg_ptr->reg_src_number ||
			addr->head.datalength == reg_ptr->reg_len) {
			*rgt = reg_ptr;
			return S_OK;
		}
		reg_ptr++;
	}

	return S_FAIL;
}

int dump_addrdata(struct spu_addrdata addr) {
	eprintf("addrhead: <0x%02x>;", *(unsigned char*)&addr.head);
	if (addr.head.source == SPU_DSRC_DIRECT) {
		eprintf(" number: <0x");
		uint64_t number = addr.direct_number;
		uint8_t *num_iter = (uint8_t *)&number;

		for (size_t i = 0; i < (1 << addr.head.datalength); i++) {
			
			eprintf("%02x", num_iter[i]);
		}
		eprintf(">");
	}

	eprintf("\n");

	return 0;
}

static int parse_le_raw_number(const char *arg, uint64_t *dist_number) {
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

	*dist_number = raw_number;

	return S_OK;
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
		const struct reg_table *rgt = NULL;

		if (register_lookup(arg, arglen, &rgt)) {
			return S_FAIL;
		}

		addrhead.deref = deref > 0;
		if ((rgt->flags & REGTABLE_ARG_EXTENSION)) {
			addrhead.source = rgt->reg_src_number;
		} else {
			log_error("Sorry, not implemented");
			return S_FAIL;
		}

		addrhead.datalength = rgt->reg_len;
		memcpy(addr, &addrhead, sizeof(addrhead));

		addr->head = addrhead;

		return S_OK;
	} else if (arg[0] == '$') {
		arg++;
		arglen--;
	

		struct spu_addrhead addrhead = {0};
		addrhead.source = SPU_DSRC_DIRECT;
		addrhead.datalength = SPU_DLEN_8BYTE;
		addrhead.deref = deref > 0;
		
		uint64_t raw_number = 0;
		if (parse_le_raw_number(arg, &raw_number)) {
			return S_FAIL;
		}

		addr->head = addrhead;
		addr->direct_number = raw_number;

		return S_OK;
	}

	return S_FAIL;
}

ssize_t addr_to_str(struct spu_addrdata addr,
		  char buf[], size_t buflen) {
	assert (buf);
	
	size_t left_len = buflen;
	char *cur_buf = buf;
	
	if (addr.head.deref) {
		if (left_len < 2) {
			return -1;
		}

		*(cur_buf++) = '*';
		*(cur_buf++) = '(';
		left_len -= 2;
	}

	if (addr.head.source == SPU_DSRC_DIRECT) {
		uint64_t raw_number = le64toh(addr.direct_number);
		int written = snprintf(cur_buf, left_len, 
			"&0x%0lx", raw_number);
		
		if (	written < 0 || 
			(size_t) written >= left_len) {
			return S_FAIL;
		}
		left_len -= (size_t) written;
		cur_buf  += (size_t) written;
	} else {
		const struct reg_table *rgt = NULL;
		if (addr_to_reg_lookup(&addr, &rgt)) {
			return S_FAIL;
		}

		int written = snprintf(cur_buf, left_len, 
			"%%%s", rgt->reg_name);
		
		if (	written < 0 ||
			(size_t) written >= left_len) {
			return S_FAIL;
		}
		left_len -= (size_t) written;
		cur_buf  += (size_t) written;
	}

	if (addr.head.deref) {
		if (left_len < 1) {
			return S_FAIL;
		}
		*(cur_buf++) = ')';
		left_len--;
	}

	return (ssize_t)(buflen - left_len);
}

int write_raw_addr(struct spu_addrdata addr, 
		   uint8_t addrbuf[sizeof(struct spu_addrdata)],
		   size_t *addrbuf_len) {
	assert (addrbuf);
	assert (addrbuf_len);

	*addrbuf_len = 0;
	memcpy(addrbuf, &addr.head, sizeof(addr.head));
	*addrbuf_len += sizeof(addr.head);

	size_t dirnum_len = (1 << addr.head.datalength);
	memcpy(	addrbuf + *addrbuf_len, 
		&addr.direct_number,
		dirnum_len
	);

	*addrbuf_len += dirnum_len;

	return S_OK;
}
