#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>

// Recursively walks a raw EVIO bank buffer, calling `visit` for every bank
// encountered (both container and leaf banks).
//
// `words` should point at the start of a bank's header (its length word);
// `n_words` is how many words are available to read.
//
// visit(tag, type, num, depth, payload_words, payload_n_words):
//   payload_words/payload_n_words point at the bank's *content*, i.e. just
//   past its 2-word header -- this is what you hand to a leaf decoder
//   like decode_mpd_word_stream() once you've identified a matching tag.
using EvioBankVisitor = std::function<void(
    uint16_t tag, uint8_t type, uint8_t num, int depth,
    const uint32_t* payload_words, size_t payload_n_words)>;

void walk_evio_banks(const uint32_t* words, size_t n_words, int depth,
                      const EvioBankVisitor& visit);
