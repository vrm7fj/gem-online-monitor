cmake_minimum_required(VERSION 3.14)
project(gem_online_monitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_ET_DUMP "Build the live ET bank-discovery tool" ON)
option(BUILD_TESTS "Build unit tests" ON)

add_library(evio_walk STATIC evio_walk.cpp)
target_include_directories(evio_walk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTS)
    enable_testing()
    add_executable(test_evio_walk test_evio_walk.cpp)
    target_link_libraries(test_evio_walk PRIVATE evio_walk)
    add_test(NAME evio_walk_test COMMAND test_evio_walk)
endif()

if(BUILD_ET_DUMP)
    find_path(ET_INCLUDE_DIR et.h
        HINTS ${CODA_ROOT} ENV CODA
        PATH_SUFFIXES common/include et/include include
    )
    find_library(ET_LIBRARY NAMES et
        HINTS ${CODA_ROOT} ENV CODA
        PATH_SUFFIXES Linux-x86_64/lib Linux-aarch64/lib linux-x86_64/lib
                      common/lib et/lib lib64 lib
    )

    if(ET_INCLUDE_DIR AND ET_LIBRARY)
        add_executable(et_dump et_dump.cpp)
        target_include_directories(et_dump PRIVATE ${ET_INCLUDE_DIR})
        target_link_libraries(et_dump PRIVATE evio_walk ${ET_LIBRARY})
        message(STATUS "et_dump: enabled")
        message(STATUS "  ET include: ${ET_INCLUDE_DIR}")
        message(STATUS "  ET library: ${ET_LIBRARY}")
    else()
        message(WARNING
            "et_dump skipped because CODA ET was not found.\n"
            "  CODA_ROOT='${CODA_ROOT}'\n"
            "  ET_INCLUDE_DIR='${ET_INCLUDE_DIR}'\n"
            "  ET_LIBRARY='${ET_LIBRARY}'\n"
            "Pass -DCODA_ROOT=/actual/coda/path, or pass ET paths directly:\n"
            "  -DET_INCLUDE_DIR=/path/to/include -DET_LIBRARY=/path/to/libet.so"
        )
    endif()
endif()
