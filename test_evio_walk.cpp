// Sanity test for walk_evio_banks() using a hand-built nested EVIO buffer:
// physics event bank (tag=1, container)
//   -> ROC bank (tag=42, container)
//        -> leaf bank (tag=0x0DE9, raw data, 3 payload words)
//
// Build: g++ -O0 -g -std=c++17 -I../include test_evio_walk.cpp \
//            ../src/evio_walk.cpp -o test_evio_walk
// Run:   ./test_evio_walk

#include <cassert>
#include <cstdio>
#include <vector>

#include "evio_walk.h"

static uint32_t bank_header(uint16_t tag, uint8_t type, uint8_t num) {
    return (static_cast<uint32_t>(tag) << 16) | (static_cast<uint32_t>(type) << 8) | num;
}

int main() {
    // Leaf bank: tag=0x0DE9, type=0x01 (raw data), 3 payload words.
    // length = 1 (header word) + payload word count.
    std::vector<uint32_t> leaf = {
        1 + 3,                           // length
        bank_header(0x0DE9, 0x01, 7),    // num=7 standing in for a module id
        111, 222, 333,
    };

    // ROC bank: tag=42, type=0x10 (container), wraps the leaf bank.
    std::vector<uint32_t> roc;
    roc.push_back(1 + static_cast<uint32_t>(leaf.size()));  // length
    roc.push_back(bank_header(42, 0x10, 0));
    roc.insert(roc.end(), leaf.begin(), leaf.end());

    // Physics event bank: tag=1, type=0x10 (container), wraps the ROC bank.
    std::vector<uint32_t> event;
    event.push_back(1 + static_cast<uint32_t>(roc.size()));  // length
    event.push_back(bank_header(1, 0x10, 0));
    event.insert(event.end(), roc.begin(), roc.end());

    int n_visited = 0;
    bool found_leaf = false;

    walk_evio_banks(event.data(), event.size(), 0,
        [&](uint16_t tag, uint8_t type, uint8_t num, int depth,
            const uint32_t* payload, size_t payload_n) {
            ++n_visited;
            std::printf("depth=%d tag=0x%04x type=0x%02x num=%u payload_n=%zu\n",
                        depth, tag, type, num, payload_n);

            if (tag == 0x0DE9) {
                found_leaf = true;
                assert(depth == 2);
                assert(num == 7);
                assert(payload_n == 3);
                assert(payload[0] == 111 && payload[1] == 222 && payload[2] == 333);
            }
        });

    assert(n_visited == 3);  // event, roc, leaf
    assert(found_leaf);

    std::printf("OK: EVIO bank walker correctly found the nested leaf bank\n");
    return 0;
}
