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
int parse_register(const char *str, spu_register_num_t *regptr) {
	assert (str);

	int ret = S_OK;

	if (str[0] != 'r') {
		_CT_FAIL();
	}

	str++;

	{
		char *endptr = NULL;
		unsigned long rnum = strtoul(str, &endptr, 10);
		if (!(*str != '\0' && *endptr == '\0')) {
			str--;

			if (!strcmp(str, REGISTER_RSP_NAME)) {
				return REGISTER_RSP_CODE;
			}

			_CT_FAIL();
		}

		if (rnum > 30) {
			_CT_FAIL();
		}

		*regptr = (spu_register_num_t)rnum;
	}

_CT_EXIT_POINT:
	if (ret) {
		log_error("Error while parsing register <%s>", str);
	}

	return ret;
}

int parse_literal_number(const char *str, int32_t *num) {
	assert (str);
	assert (num);

	int ret = S_OK;

	if (*str != '$') {
		_CT_FAIL();
	}
	str++;

	{
		char *endptr = NULL;
		int32_t rnum = (int32_t)strtol(str, &endptr, 0);
		if (!(*str != '\0' && *endptr == '\0')) {
			_CT_FAIL();
		}

		*num = rnum;
	}

_CT_EXIT_POINT:
	if (ret) {
		log_error("Error while parsing number <%s>", str);
	}
	return ret;
}
