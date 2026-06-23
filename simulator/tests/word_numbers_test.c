#include "pages/load_mnemonic/word_numbers_logic.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  uint16_t number = 0;
  assert(word_numbers_parse_decimal("1", &number) && number == 1);
  assert(word_numbers_parse_decimal("2048", &number) && number == 2048);
  assert(!word_numbers_parse_decimal("", &number));
  assert(!word_numbers_parse_decimal("0", &number));
  assert(!word_numbers_parse_decimal("2049", &number));
  assert(!word_numbers_parse_decimal("12a", &number));

  char decimal[5];
  for (uint16_t expected = 1; expected <= 2048; expected++) {
    snprintf(decimal, sizeof(decimal), "%u", (unsigned)expected);
    assert(word_numbers_parse_decimal(decimal, &number));
    assert(number == expected);
  }

  assert(!word_numbers_digit_can_append("", 0, '0'));
  assert(word_numbers_digit_can_append("", 0, '1'));
  assert(word_numbers_digit_can_append("204", 3, '8'));
  assert(!word_numbers_digit_can_append("204", 3, '9'));
  assert(!word_numbers_digit_can_append("2048", 4, '0'));
  assert(!word_numbers_digit_can_append("", 0, 'x'));

  for (uint16_t prefix_value = 1; prefix_value <= 2048; prefix_value++) {
    snprintf(decimal, sizeof(decimal), "%u", (unsigned)prefix_value);
    size_t len = strlen(decimal);
    for (char digit = '0'; digit <= '9'; digit++) {
      bool allowed = word_numbers_digit_can_append(decimal, len, digit);
      if (len >= 4) {
        assert(!allowed);
      } else {
        unsigned appended =
            (unsigned)prefix_value * 10u + (unsigned)(digit - '0');
        assert(allowed == (appended <= 2048));
      }
    }
  }

  assert(!word_numbers_should_autocomplete("204", 3));
  assert(word_numbers_should_autocomplete("205", 3));
  assert(word_numbers_should_autocomplete("999", 3));
  assert(!word_numbers_should_autocomplete("100", 3));
  assert(word_numbers_should_autocomplete("2048", 4));

  puts("word_numbers_test: OK");
  return 0;
}
