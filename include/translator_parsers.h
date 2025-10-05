#ifndef TRANSLATOR_PARSERS_H
#define TRANSLATOR_PARSERS_H

#include <stdint.h>

int parse_register(const char *str);
int parse_literal_number(const char *str, int32_t *num);

#endif /* TRANSLATOR_PARSERS_H */
