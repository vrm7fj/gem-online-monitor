#include "evio_walk.h"
#include "evio_words.h"

void walk_evio_banks(const uint32_t* words, size_t n_words, int depth,
                      const EvioBankVisitor& visit) {
    size_t pos = 0;
    while (pos + 1 < n_words) {  // need at least the 2 header words
        uint32_t length = evio::bank_length(words[pos]);
        uint32_t header = words[pos + 1];
        uint16_t tag = evio::bank_tag(header);
        uint8_t type = evio::bank_type(header);
        uint8_t num = evio::bank_num(header);

        size_t bank_total_words = static_cast<size_t>(length) + 1;  // includes word[0]
        if (pos + bank_total_words > n_words || bank_total_words < 2) {
            break;  // malformed/truncated -- stop rather than read out of bounds
        }

        const uint32_t* payload = words + pos + 2;
        size_t payload_n = bank_total_words - 2;

        visit(tag, type, num, depth, payload, payload_n);

        if (type == evio::BANK_OF_BANKS_TYPE) {
            walk_evio_banks(payload, payload_n, depth + 1, visit);
        }

        pos += bank_total_words;
    }
}
