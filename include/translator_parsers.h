/**
 * @file
 *
 * @brief Parsers for Translator
 */

#ifndef TRANSLATOR_PARSERS_H
#define TRANSLATOR_PARSERS_H

#include <stdint.h>
#include "spu_asm.h"

int parse_register(const char *str, spu_register_num_t *regptr);
int parse_literal_number(const char *str, int32_t *num);

#endif /* TRANSLATOR_PARSERS_H */
