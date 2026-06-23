#include "stackbit_1248_codec.h"
#include <string.h>

bool stackbit_1248_encode_number(uint16_t word_number,
                                 bool marks[STACKBIT_1248_MARK_COUNT]) {
  if (!marks || word_number < 1 || word_number > 2048)
    return false;

  memset(marks, 0, sizeof(bool) * STACKBIT_1248_MARK_COUNT);
  int digits[4] = {(word_number / 1000) % 10, (word_number / 100) % 10,
                   (word_number / 10) % 10, word_number % 10};

  marks[0] = digits[0] == 1;
  marks[7] = digits[0] == 2;
  for (int digit = 1; digit < 4; digit++) {
    int top = 1 + (digit - 1) * 2;
    int bottom = 8 + (digit - 1) * 2;
    marks[top] = (digits[digit] & 1) != 0;
    marks[top + 1] = (digits[digit] & 2) != 0;
    marks[bottom] = (digits[digit] & 4) != 0;
    marks[bottom + 1] = (digits[digit] & 8) != 0;
  }
  return true;
}
