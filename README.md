# GEM Online Monitor (MPD/VTP + CODA)

Live online monitoring for GEM detector testing, reading from an ET system
fed by CODA, decoding MPD/APV25 banks in C++, and doing analysis +
visualization (2D hits, occupancy, amplitude spectra) in Python.

## Quick start: live decode to terminal (`et_dump`)

The simplest live piece in this repo. No ZMQ, no Python, no GUI -- just
attach to a real ET system, decode events as they arrive, print to stdout.
Needs only the ET library (not evio, not ZMQ):

```bash
cmake -B build -S . -DCODA_ROOT=$CODA
cmake --build build -j

# Discovery mode: find your MPD bank's tag from real data
./build/et_dump /path/to/et/system

# Decode mode: once you know the tag, decode and print hits live
./build/et_dump /path/to/et/system --tag 0xYOUR_MPD_TAG
```

Run discovery mode first against your real ET system -- it prints every
distinct bank (tag, type, num) it encounters, so you can identify which
tag is your MPD data without needing your DAQ config in hand ahead of
time. Then re-run with `--tag` to actually decode and print strip hits.

## Architecture

```
CODA ET system
      |
      v
[C++] et_publisher  (cpp/src/et_publisher.cpp)
      |  - attaches to ET as a parallel, non-blocking station
      |  - walks the EVIO event tree to find each MPD's raw data words
      |    (TODO: this part -- see below)
      |  - decode_mpd_word_stream() (cpp/src/mpd_apv_decoder.cpp) unpacks
      |    the MPD/APV25 wire format into HitRecords
      |  - packs hits into a fixed binary struct (cpp/include/hit_format.h)
      v
ZeroMQ PUB socket  (tcp://*:5556)
      |  - decouples DAQ-rate producer from GUI-rate consumer
      |  - drops events if the subscriber falls behind (fine for monitoring)
      v
[Python] subscriber.py -> analysis.py -> monitor_gui.py
      - decode binary frames into numpy structured arrays
      - pulse amplitude extraction from the 6 time samples, mode-aware
        (raw full-readout vs. firmware online zero-suppression)
      - per-plane strip clustering
      - naive X/Y cluster matching -> 2D hit points
      - occupancy accumulation
      - live matplotlib display
```

## What you need to plug in

1. **EVIO tree-walking** in `decode_mpd_banks()` (`cpp/src/et_publisher.cpp`)
   — find each MPD's raw data words within the event, using JLab's evio
   C++ API (see the User's Guide / Doxygen linked from
   [JeffersonLab/evio](https://github.com/JeffersonLab/evio)). This is
   deliberately left as an integration point rather than guessed, since the
   bank tags and ROC layout are specific to your DAQ config. Once you have,
   per MPD, its module id and a pointer/length to its raw word array, the
   rest of the decode is already implemented (see next item).

2. **MPD/APV25 bit-level decoding** — `decode_mpd_word_stream()`
   (`cpp/src/mpd_apv_decoder.cpp`) is a full implementation of the
   MPD/APV25 tagged-word wire format: block/event headers and trailers,
   APV frame headers, per-channel sample words, and APV trailers. Field
   layout is documented in `cpp/include/mpd_words.h`, and
   `cpp/test/test_mpd_decoder.cpp` verifies it against a hand-built
   synthetic word stream (2 channels x 3 samples). This word format is a
   fixed MPD firmware protocol (see JLab's `dataDecode` and `mpdApvDecode`
   repos for other independent implementations of the same hardware
   format) — the accessors here are a fresh implementation using explicit
   bit shifts/masks rather than C bitfields, since bitfield packing order
   isn't portable across compilers.

3. **Channel-to-plane mapping** — `plane_for_apv()` in `et_publisher.cpp`
   is a placeholder (even/odd APV split). The raw words only carry
   `apv_id`; mapping that to "which plane" is a detector-geometry decision
   that needs your actual GEM/APV layout.

4. **`match_2d_hits()`** in `python/analysis.py` is a placeholder that
   pairs X/Y clusters by amplitude rank only. Replace with real
   position/timing-based matching once you're ready — especially important
   if you expect multiple clusters per event.

5. **Pedestals** — `PulseProcessor` currently uses `min(samples)` as a
   per-hit baseline for APVs it detects as full/raw readout. Swap in a real
   per-channel pedestal map (`{(mpd_id, apv_id, channel): baseline}`) once
   you have one from a pedestal run. `PulseProcessor` will print a one-time
   warning if it sees raw data with no pedestal map loaded.

   `PulseProcessor` also auto-detects, per APV per event, whether that APV
   is running full readout (all 128 strips sent -- needs offline
   pedestal/CM subtraction) or online zero-suppression (fewer strips sent --
   firmware already did pedestal/CM/threshold suppression, so the samples
   can be used as-is). This mirrors standard MPD/APV25 firmware behavior;
   worth double-checking the strip-count threshold against your own
   firmware config. (Thanks to JLab's `prad2evviewer` GEM notes for
   flagging this as a thing to handle -- the detection logic here is our
   own, not derived from their code.)

6. **`N_CHANNELS`** in `monitor_gui.py` and `OccupancyTracker` — set to your
   actual strips-per-plane count.

## Running it

```bash
# 0. Optional: sanity-check the MPD word decoder in isolation first
cd cpp/test
g++ -O0 -g -std=c++17 -I../include test_mpd_decoder.cpp ../src/mpd_apv_decoder.cpp \
    -o test_mpd_decoder
./test_mpd_decoder

# 1. Build and run the C++ side (adjust paths to your CODA/ET/ZMQ install)
cd ../src
g++ -O2 -std=c++17 et_publisher.cpp mpd_apv_decoder.cpp -I../include \
    -I$CODA/et/include -L$CODA/et/lib -let -lzmq -o et_publisher
./et_publisher /path/to/et/system

# 2. In another shell, run the Python monitor
cd ../../python
pip install -r requirements.txt
python monitor_gui.py tcp://<host-running-publisher>:5556
```

`subscriber.py` can also be run standalone (`python subscriber.py`) as a
quick smoke test of event rate and hit counts, without the GUI.

## Design notes / things worth deciding early

- **Where pulse processing happens**: right now amplitude extraction is in
  Python for flexibility while you're prototyping. If it becomes a
  bottleneck at full DAQ rate, move it into `decode_mpd_banks()` in C++ and
  send derived amplitude/time instead of raw samples.
- **Wire format**: fixed-size binary struct chosen over
  JSON/msgpack/protobuf for near-zero serialization overhead — numpy can
  reinterpret the raw bytes directly via `np.frombuffer`, no parsing loop.
  If you ever need a more flexible/self-describing format (e.g. optional
  fields, variable schema), consider switching to a schema-based
  serializer like FlatBuffers.
- **PUB/SUB vs PUSH/PULL**: PUB/SUB was chosen so multiple independent
  consumers (e.g. a live GUI + a separate rate logger) can subscribe to the
  same stream without competing for events.
