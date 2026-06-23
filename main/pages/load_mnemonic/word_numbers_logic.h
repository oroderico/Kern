#ifndef WORD_NUMBERS_LOGIC_H
#define WORD_NUMBERS_LOGIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool word_numbers_parse_decimal(const char *text, uint16_t *number_out);
bool word_numbers_digit_can_append(const char *prefix, size_t prefix_len,
                                   char digit);
bool word_numbers_should_autocomplete(const char *text, size_t text_len);

#endif // WORD_NUMBERS_LOGIC_H
