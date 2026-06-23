#ifndef STACKBIT_1248_CODEC_H
#define STACKBIT_1248_CODEC_H

#include <stdbool.h>
#include <stdint.h>

#define STACKBIT_1248_MARK_COUNT 14

// Convert a 1-based BIP39 word number into the 14 punch positions used by a
// Stackbit 1248 plate. Returns false for values outside the BIP39 range.
bool stackbit_1248_encode_number(uint16_t word_number,
                                 bool marks[STACKBIT_1248_MARK_COUNT]);

#endif // STACKBIT_1248_CODEC_H
