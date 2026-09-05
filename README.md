# GEM CODA ET Bank Monitor

Small C++17 tools for inspecting the EVIO bank structure of events from a
running CODA ET system. The monitor attaches through a non-blocking ET station
and returns each event after inspection.

The current repository provides bank discovery, not MPD/APV sample decoding.

## Build the dependency-free test

```sh
cmake -S . -B build -DBUILD_ET_DUMP=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Build `et_dump` with CODA ET

First verify that `CODA` points to a real directory:

```sh
echo "$CODA"
find "$CODA" -name et.h -o -name 'libet.so'
```

Then configure and build:

```sh
cmake -S . -B build -DCODA_ROOT="$CODA"
cmake --build build -j
```

If CODA uses a nonstandard layout, pass the two paths directly:

```sh
cmake -S . -B build \
  -DET_INCLUDE_DIR=/path/to/directory/containing/et.h \
  -DET_LIBRARY=/path/to/libet.so
cmake --build build -j
```

Configuration must print `et_dump: enabled`. If it says `et_dump skipped`, do
not run the previous build; correct the ET paths and configure again.

## Run

With CODA already running, obtain the active ET system-file path from the DAQ
configuration or operator, then run:

```sh
./build/et_dump /path/to/et/system
```

Stop the monitor with Ctrl-C. It creates a station named `gem_et_dump`.

## Contents

- `evio_words.h`: EVIO bank-header accessors
- `evio_walk.h`, `evio_walk.cpp`: bounded recursive EVIO bank walker
- `test_evio_walk.cpp`: dependency-free synthetic event test
- `et_dump.cpp`: live ET consumer and EVIO bank discovery utility
