#include "pages/home/backup/stackbit_1248_codec.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static void expect_marks(uint16_t number, const int *expected,
                         size_t expected_count) {
  bool marks[STACKBIT_1248_MARK_COUNT];
  assert(stackbit_1248_encode_number(number, marks));

  for (size_t i = 0; i < STACKBIT_1248_MARK_COUNT; i++) {
    bool should_be_marked = false;
    for (size_t j = 0; j < expected_count; j++) {
      if ((size_t)expected[j] == i) {
        should_be_marked = true;
        break;
      }
    }
    assert(marks[i] == should_be_marked);
  }
}

int main(void) {
  const int one[] = {5};
  const int nine[] = {5, 13};
  const int ten[] = {3};
  const int example_1268[] = {0, 2, 4, 10, 13};
  const int last_word[] = {7, 10, 13};

  expect_marks(1, one, sizeof(one) / sizeof(one[0]));
  expect_marks(9, nine, sizeof(nine) / sizeof(nine[0]));
  expect_marks(10, ten, sizeof(ten) / sizeof(ten[0]));
  expect_marks(1268, example_1268,
               sizeof(example_1268) / sizeof(example_1268[0]));
  expect_marks(2048, last_word, sizeof(last_word) / sizeof(last_word[0]));

  bool marks[STACKBIT_1248_MARK_COUNT];
  assert(!stackbit_1248_encode_number(0, marks));
  assert(!stackbit_1248_encode_number(2049, marks));
  assert(!stackbit_1248_encode_number(1, NULL));

  puts("stackbit_1248_test: OK");
  return 0;
}
