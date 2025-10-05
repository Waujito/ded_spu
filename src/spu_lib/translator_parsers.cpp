#include <stdlib.h>
#include <string.h>
#include <assert.h>


#include "types.h"
#include "spu_asm.h"
#include "translator_parsers.h"

/**
 * Parses a register.
 * Returns the number of register or -1 on error.
 */ 
int parse_register(const char *str) {
	if (str[0] != 'r') {
		return S_FAIL;
	}

	str++;

	char *endptr = NULL;
	unsigned long rnum = strtoul(str, &endptr, 10);
	if (!(*str != '\0' && *endptr == '\0')) {
		str--;

		if (!strcmp(str, REGISTER_RSP_NAME)) {
			return REGISTER_RSP_CODE;
		}

		return S_FAIL;
	}

	if (rnum > 30) {
		return S_FAIL;
	}

	return (int) rnum;
}

int parse_literal_number(const char *str, int32_t *num) {
	assert (str);
	assert (num);

	if (*str != '$') {
		return S_FAIL;
	}
	str++;

	char *endptr = NULL;
	int32_t rnum = (int32_t)strtol(str, &endptr, 0);
	if (!(*str != '\0' && *endptr == '\0')) {
		return S_FAIL;
	}

	*num = rnum;

	return S_OK;
}
