// Live CODA ET consumer -- no ZMQ, no Python, no GUI. Attaches to a real
// ET system, EVIO-decodes each event as it arrives, and prints to the
// terminal. Two modes:
//
//   et_dump <et_system_file> [--max-events N]
//       Discovery mode: prints each newly-seen (tag, type, num) bank as
//       it's encountered, so you can find your MPD bank's tag from real
//       data without already knowing your DAQ config.
//
// Build (adjust to wherever ET actually lives on your system):
//   g++ -O2 -std=c++17 et_dump.cpp evio_walk.cpp \
//       -I$CODA/common/include -L$CODA/Linux-x86_64/lib -let -o et_dump

#include <cstdio>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <limits>
#include <string>
#include <set>
#include <iostream>
#include <unistd.h>

#include <et.h>

#include "evio_walk.h"

namespace {
volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int) {
    stop_requested = 1;
}

bool parse_max_events(const char* text, uint64_t& result) {
    if (text == nullptr || *text == '\0' || *text == '-') return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0' || value == 0) return false;
    result = static_cast<uint64_t>(value);
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        std::cerr << "usage: " << argv[0]
                  << " <et_system_file> [--max-events N]\n";
        return 1;
    }
    std::string et_filename = argv[1];
    uint64_t max_events = std::numeric_limits<uint64_t>::max();
    if (argc == 4 &&
        (std::strcmp(argv[2], "--max-events") != 0 ||
         !parse_max_events(argv[3], max_events))) {
        std::cerr << "--max-events requires a positive integer\n";
        return 1;
    }

    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    // A serial station at ET_END sees the normal stream. NONBLOCKING and a
    // short cue let events bypass it if this monitor cannot keep up.
    et_sys_id sys_id;
    et_openconfig openconfig;
    int status = et_open_config_init(&openconfig);
    if (status != ET_OK) {
        std::cerr << "et_open_config_init failed: " << status << "\n";
        return 1;
    }
    status = et_open(&sys_id, et_filename.c_str(), openconfig);
    et_open_config_destroy(openconfig);
    if (status != ET_OK) {
        std::cerr << "et_open failed for " << et_filename << "\n";
        return 1;
    }

    et_statconfig statconfig;
    status = et_station_config_init(&statconfig);
    if (status != ET_OK) {
        std::cerr << "et_station_config_init failed: " << status << "\n";
        et_close(sys_id);
        return 1;
    }
    et_station_config_setflow(statconfig, ET_STATION_SERIAL);
    et_station_config_setblock(statconfig, ET_STATION_NONBLOCKING);
    et_station_config_setselect(statconfig, ET_STATION_SELECT_ALL);
    et_station_config_setrestore(statconfig, ET_STATION_RESTORE_OUT);
    et_station_config_setcue(statconfig, 10);

    et_stat_id stat_id;
    const std::string station_name = "gem_et_dump_" + std::to_string(getpid());
    status = et_station_create(sys_id, &stat_id, station_name.c_str(), statconfig);
    et_station_config_destroy(statconfig);
    if (status != ET_OK) {
        std::cerr << "et_station_create failed: " << status << "\n";
        et_close(sys_id);
        return 1;
    }

    et_att_id att_id;
    status = et_station_attach(sys_id, stat_id, &att_id);
    if (status != ET_OK) {
        std::cerr << "et_station_attach failed: " << status << "\n";
        et_station_remove(sys_id, stat_id);
        et_close(sys_id);
        return 1;
    }

    std::printf("Connected to %s with station %s.\n", et_filename.c_str(),
                station_name.c_str());
    std::printf("Discovery mode: printing each newly-seen EVIO bank tag.\n");
    std::printf("Press Ctrl-C to stop cleanly.\n\n");

    std::set<uint32_t> seen;  // (tag<<16 | type<<8 | num) already printed, discovery mode only
    uint64_t event_number = 0;

    et_event* ev_array[1];
    timespec timeout = {1, 0};
    while (!stop_requested && event_number < max_events) {
        int nread = 0;
        status = et_events_get(sys_id, att_id, ev_array, ET_TIMED,
                               &timeout, 1, &nread);
        if (status == ET_ERROR_TIMEOUT) continue;
        if (status != ET_OK) {
            std::cerr << "et_events_get failed: " << status << "\n";
            break;
        }
        if (nread != 1) continue;

        void* data = nullptr;
        size_t len_bytes = 0;
        const int data_status = et_event_getdata(ev_array[0], &data);
        const int length_status = et_event_getlength(ev_array[0], &len_bytes);

        if (data_status == ET_OK && length_status == ET_OK && data != nullptr) {
            const uint32_t* words = static_cast<const uint32_t*>(data);
            size_t n_words = len_bytes / sizeof(uint32_t);

            walk_evio_banks(words, n_words, /*depth=*/0,
                [&](uint16_t tag, uint8_t type, uint8_t num, int depth,
                    const uint32_t*, size_t payload_n) {

                    uint32_t key = (static_cast<uint32_t>(tag) << 16)
                                 | (static_cast<uint32_t>(type) << 8) | num;
                    if (seen.insert(key).second) {
                        std::printf(
                            "event %llu: bank tag=0x%04x type=0x%02x num=%u depth=%d words=%zu\n",
                            static_cast<unsigned long long>(event_number),
                            tag, type, num, depth, payload_n);
                    }
                });
        } else {
            std::cerr << "Could not read ET event metadata\n";
        }

        status = et_events_put(sys_id, att_id, ev_array, 1);
        if (status != ET_OK) {
            std::cerr << "et_events_put failed: " << status << "\n";
            break;
        }
        ++event_number;
    }

    std::printf("\nRead %llu events; detaching station.\n",
                static_cast<unsigned long long>(event_number));
    const int detach_status = et_station_detach(sys_id, att_id);
    const int remove_status = et_station_remove(sys_id, stat_id);
    const int close_status = et_close(sys_id);
    if (detach_status != ET_OK || remove_status != ET_OK || close_status != ET_OK) {
        std::cerr << "ET cleanup status: detach=" << detach_status
                  << " remove=" << remove_status << " close=" << close_status << "\n";
        return 1;
    }
    return status == ET_OK || status == ET_ERROR_TIMEOUT ? 0 : 1;
}
