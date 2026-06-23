#include "word_numbers_logic.h"

bool word_numbers_parse_decimal(const char *text, uint16_t *number_out) {
  if (!text || !number_out || text[0] == '\0')
    return false;

  unsigned value = 0;
  for (size_t i = 0; text[i] != '\0'; i++) {
    if (text[i] < '0' || text[i] > '9')
      return false;
    value = value * 10u + (unsigned)(text[i] - '0');
    if (value > 2048)
      return false;
  }
  if (value == 0)
    return false;

  *number_out = (uint16_t)value;
  return true;
}

bool word_numbers_digit_can_append(const char *prefix, size_t prefix_len,
                                   char digit) {
  if (!prefix || digit < '0' || digit > '9' || prefix_len >= 4)
    return false;
  if (prefix_len == 0 && digit == '0')
    return false;

  unsigned value = 0;
  for (size_t i = 0; i < prefix_len; i++) {
    if (prefix[i] < '0' || prefix[i] > '9')
      return false;
    value = value * 10u + (unsigned)(prefix[i] - '0');
  }
  value = value * 10u + (unsigned)(digit - '0');
  return value <= 2048;
}

bool word_numbers_should_autocomplete(const char *text, size_t text_len) {
  uint16_t number = 0;
  if (!word_numbers_parse_decimal(text, &number))
    return false;
  return text_len == 4 || (text_len == 3 && number > 204);
}
