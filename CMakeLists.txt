cmake_minimum_required(VERSION 3.14)
project(gem_online_monitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_ET_PUBLISHER "Build the ET->ZMQ publisher (needs evio, ET, ZeroMQ)" ON)
option(BUILD_ET_DUMP "Build the live ET->terminal decoder (needs ET only)" ON)
option(BUILD_TESTS "Build the standalone decoder unit tests" ON)

# --------------------------------------------------------------------------
# mpd_decode: the MPD/APV25 word decoder plus the EVIO bank-tree walker.
# Zero external dependencies -- both are pure bit-twiddling over an
# in-memory word buffer, no evio/ET/ZMQ library needed. This is the part
# every consumer of this repo actually wants; everything else here is
# optional plumbing around it.
# --------------------------------------------------------------------------
add_library(mpd_decode STATIC
    cpp/src/mpd_apv_decoder.cpp
    cpp/src/evio_walk.cpp
)
target_include_directories(mpd_decode PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
    $<INSTALL_INTERFACE:include>
)

# --------------------------------------------------------------------------
# mpd_dump: minimal standalone CLI to decode raw MPD words and print hits
# to the terminal. Dependency-free (just mpd_decode) -- a bench harness for
# the decoder that needs no evio/ET/ZMQ/live DAQ to be useful.
# --------------------------------------------------------------------------
add_executable(mpd_dump cpp/src/mpd_dump.cpp)
target_link_libraries(mpd_dump PRIVATE mpd_decode)

# --------------------------------------------------------------------------
# Standalone unit tests -- also dependency-free, so they always build.
# --------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()

    add_executable(test_mpd_decoder cpp/test/test_mpd_decoder.cpp)
    target_link_libraries(test_mpd_decoder PRIVATE mpd_decode)
    add_test(NAME mpd_decoder_test COMMAND test_mpd_decoder)

    add_executable(test_evio_walk cpp/test/test_evio_walk.cpp)
    target_link_libraries(test_evio_walk PRIVATE mpd_decode)
    add_test(NAME evio_walk_test COMMAND test_evio_walk)
endif()

# --------------------------------------------------------------------------
# et_dump: live ET consumer that decodes real CODA events and prints hits
# (or discovered bank tags) straight to the terminal. Needs only ET --
# EVIO's bank-header word format is parsed directly by evio_walk.cpp, no
# evio library required. No ZMQ, no Python, no GUI.
#
# Configure example:
#   cmake -B build -DCODA_ROOT=$CODA
# --------------------------------------------------------------------------
if(BUILD_ET_DUMP)
    find_path(ET_INCLUDE_DIR et.h
        HINTS ${CODA_ROOT} ENV CODA
        PATH_SUFFIXES et/include include
    )
    find_library(ET_LIBRARY NAMES et
        HINTS ${CODA_ROOT} ENV CODA
        PATH_SUFFIXES et/lib lib
    )

    if(ET_INCLUDE_DIR AND ET_LIBRARY)
        add_executable(et_dump cpp/src/et_dump.cpp)
        target_link_libraries(et_dump PRIVATE mpd_decode ${ET_LIBRARY})
        target_include_directories(et_dump PRIVATE ${ET_INCLUDE_DIR})
        message(STATUS "et_dump: will be built (ET found)")
    else()
        message(WARNING
            "et_dump: skipped -- ET not found.\n"
            "  ET_INCLUDE_DIR = ${ET_INCLUDE_DIR}\n"
            "  ET_LIBRARY     = ${ET_LIBRARY}\n"
            "Set -DCODA_ROOT= (or export \$CODA) and re-run cmake."
        )
    endif()
endif()

# --------------------------------------------------------------------------
# et_publisher: needs evio, ET, and ZeroMQ. These aren't things find_package
# knows about out of the box on most systems, so we search explicit hints
# (cache variables / environment variables you set at configure time) and
# simply skip this target -- with a clear message -- if any are missing.
# The mpd_decode library and its test above are unaffected either way.
#
# Configure examples:
#   cmake -B build -DEVIO_ROOT=/path/to/evio/install \
#                   -DCODA_ROOT=$CODA \
#                   -DCPPZMQ_ROOT=/path/to/cppzmq/checkout
# --------------------------------------------------------------------------
if(BUILD_ET_PUBLISHER)
    find_path(CPPZMQ_INCLUDE_DIR zmq.hpp
        HINTS ${CPPZMQ_ROOT} ENV CPPZMQ_ROOT
    )
    find_library(ZMQ_LIBRARY NAMES zmq libzmq)
    find_path(ZMQ_INCLUDE_DIR zmq.h)

    find_path(EVIO_INCLUDE_DIR evioUtil.hxx
        HINTS ${EVIO_ROOT} ENV EVIO_ROOT
        PATH_SUFFIXES include
    )
    find_library(EVIO_LIBRARY NAMES evio
        HINTS ${EVIO_ROOT} ENV EVIO_ROOT
        PATH_SUFFIXES lib lib64
    )

    find_path(ET_INCLUDE_DIR et.h
        HINTS ${CODA_ROOT} ENV CODA
        PATH_SUFFIXES et/include include
    )
    find_library(ET_LIBRARY NAMES et
        HINTS ${CODA_ROOT} ENV CODA
        PATH_SUFFIXES et/lib lib
    )

    set(_et_publisher_deps_found TRUE)
    foreach(_dep CPPZMQ_INCLUDE_DIR ZMQ_LIBRARY ZMQ_INCLUDE_DIR
                 EVIO_INCLUDE_DIR EVIO_LIBRARY ET_INCLUDE_DIR ET_LIBRARY)
        if(NOT ${_dep})
            set(_et_publisher_deps_found FALSE)
        endif()
    endforeach()

    if(_et_publisher_deps_found)
        add_executable(et_publisher cpp/src/et_publisher.cpp)
        target_link_libraries(et_publisher PRIVATE
            mpd_decode ${EVIO_LIBRARY} ${ET_LIBRARY} ${ZMQ_LIBRARY}
        )
        target_include_directories(et_publisher PRIVATE
            ${CPPZMQ_INCLUDE_DIR} ${EVIO_INCLUDE_DIR} ${ET_INCLUDE_DIR} ${ZMQ_INCLUDE_DIR}
        )
        message(STATUS "et_publisher: will be built (evio/ET/ZMQ all found)")
    else()
        message(WARNING
            "et_publisher: skipped -- not all of evio/ET/ZMQ/cppzmq were found.\n"
            "  CPPZMQ_INCLUDE_DIR = ${CPPZMQ_INCLUDE_DIR}\n"
            "  ZMQ_LIBRARY         = ${ZMQ_LIBRARY}\n"
            "  ZMQ_INCLUDE_DIR     = ${ZMQ_INCLUDE_DIR}\n"
            "  EVIO_INCLUDE_DIR    = ${EVIO_INCLUDE_DIR}\n"
            "  EVIO_LIBRARY        = ${EVIO_LIBRARY}\n"
            "  ET_INCLUDE_DIR      = ${ET_INCLUDE_DIR}\n"
            "  ET_LIBRARY          = ${ET_LIBRARY}\n"
            "Set -DEVIO_ROOT=, -DCODA_ROOT= (or export \$CODA), -DCPPZMQ_ROOT= "
            "and re-run cmake. mpd_decode and its unit test still build fine "
            "without any of this."
        )
    endif()
endif()

# --------------------------------------------------------------------------
# Install rules
# --------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS mpd_decode
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY cpp/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(TARGET et_publisher)
    install(TARGETS et_publisher RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
