#ifndef T_DEBUG_H
#define T_DEBUG_H

#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include "types.h"

static int buf_dump_hex(void *buf, size_t buflen, FILE *out_stream) {
	assert (buf);
	assert (out_stream);

	for (size_t i = 0; i < buflen; i++) {
		fprintf(out_stream, "%02x", *(((uint8_t *)buf) + i));
	}

	return S_OK;
}

#endif /* T_DEBUG_H */
