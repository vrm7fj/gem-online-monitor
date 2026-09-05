#pragma once
#include <cstdint>

// Raw EVIO bank header word format (evio 2/3/4 wire format). This is the
// open EVIO specification itself, not any one project's code -- any
// correct reader necessarily parses it this way.
//
// Every evio "bank" is at least 2 words:
//   word[0] = length: number of words following word[0] in this bank
//             (so total bank size in words is length + 1)
//   word[1] = header word:
//       bits [31:16] tag
//       bits [15:14] padding (unused here)
//       bits [13:8]  type   (content type -- 0x10 = bank of banks, i.e. a
//                            container to recurse into; other values are
//                            leaf/data types, e.g. raw uint32 arrays)
//       bits [7:0]   num
//
// Segments/tagsegments use a different 1-word header, but the top-level
// CODA physics event and ROC banks are BANK type, which is all that's
// needed to walk down to MPD data banks nested underneath them.

namespace evio {

inline uint32_t bits(uint32_t w, int lo, int width) {
    return (w >> lo) & ((1u << width) - 1u);
}

inline uint32_t bank_length(uint32_t word0) { return word0; }
inline uint16_t bank_tag(uint32_t word1)    { return static_cast<uint16_t>(bits(word1, 16, 16)); }
inline uint8_t  bank_type(uint32_t word1)   { return static_cast<uint8_t>(bits(word1, 8, 6)); }
inline uint8_t  bank_num(uint32_t word1)    { return static_cast<uint8_t>(bits(word1, 0, 8)); }

constexpr uint8_t BANK_OF_BANKS_TYPE = 0x10;  // container: recurse into contents

} // namespace evio
