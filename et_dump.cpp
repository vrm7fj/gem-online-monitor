// Live CODA ET consumer -- no ZMQ, no Python, no GUI. Attaches to a real
// ET system, EVIO-decodes each event as it arrives, and prints to the
// terminal. Two modes:
//
//   et_dump <et_system_file>
//       Discovery mode: prints each newly-seen (tag, type, num) bank as
//       it's encountered, so you can find your MPD bank's tag from real
//       data without already knowing your DAQ config.
//
// ET API calls are illustrative -- check them against your CODA/ET
// version's et.h; station config args and et_events_get signature can
// differ slightly between versions.
//
// Build (adjust to wherever ET actually lives on your system):
//   g++ -O2 -std=c++17 et_dump.cpp evio_walk.cpp \
//       -I$CODA/common/include -L$CODA/Linux-x86_64/lib -let -o et_dump

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <set>
#include <iostream>

#include <et.h>

#include "evio_walk.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <et_system_file>\n";
        return 1;
    }
    std::string et_filename = argv[1];

    // --- ET setup: parallel, non-blocking station so this never steals or
    // slows down events for the main CODA data-recording chain. ---
    et_sys_id sys_id;
    et_openconfig openconfig;
    et_open_config_init(&openconfig);
    if (et_open(&sys_id, et_filename.c_str(), openconfig) != ET_OK) {
        std::cerr << "et_open failed for " << et_filename << "\n";
        return 1;
    }
    et_open_config_destroy(openconfig);

    et_statconfig statconfig;
    et_station_config_init(&statconfig);
    et_station_config_setblock(statconfig, ET_STATION_NONBLOCKING);
    et_station_config_setselect(statconfig, ET_STATION_SELECT_ALL);

    et_stat_id stat_id;
    et_station_create(sys_id, &stat_id, "gem_et_dump", statconfig);
    et_station_config_destroy(statconfig);

    et_att_id att_id;
    et_station_attach(sys_id, stat_id, &att_id);

    std::printf("Discovery mode: printing each newly-seen EVIO bank tag.\n\n");

    std::set<uint32_t> seen;  // (tag<<16 | type<<8 | num) already printed, discovery mode only
    uint32_t event_number = 0;

    et_event* ev_array[1];
    while (true) {
        int status = et_events_get(sys_id, att_id, ev_array, ET_SLEEP, nullptr, 1, nullptr);
        if (status != ET_OK) {
            continue;
        }

        void* data = nullptr;
        size_t len_bytes = 0;
        et_event_getdata(ev_array[0], &data);
        et_event_getlength(ev_array[0], &len_bytes);

        const uint32_t* words = static_cast<const uint32_t*>(data);
        size_t n_words = len_bytes / sizeof(uint32_t);

        walk_evio_banks(words, n_words, /*depth=*/0,
            [&](uint16_t tag, uint8_t type, uint8_t num, int depth,
                const uint32_t* payload, size_t payload_n) {

                uint32_t key = (static_cast<uint32_t>(tag) << 16)
                             | (static_cast<uint32_t>(type) << 8) | num;
                if (seen.insert(key).second) {
                    std::printf(
                        "event %u: bank tag=0x%04x type=0x%02x num=%u depth=%d words=%zu\n",
                        event_number, tag, type, num, depth, payload_n);
                }
            });

        et_events_put(sys_id, att_id, ev_array, 1);
        ++event_number;
    }

    return 0;
}
