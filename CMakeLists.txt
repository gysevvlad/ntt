cmake_minimum_required(VERSION 3.22)

project(ntt)

set(NTT_SOURCES
  src/combine.c
  src/log.c
  src/loop.c
  src/malloc.c
  src/queue.c
  src/task.c
  src/worker.c
)

set(NTT_TEST_SOURCES
  src/combine_test.cpp
  src/list_test.cpp
  src/loop_test.cpp
  src/queue_test.cpp
  src/task_list_test.cpp
  src/task_test.cpp
  src/worker_test.cpp
)

add_executable(ntt_unit_tests)

set_target_properties(ntt_unit_tests
  PROPERTIES
    C_STANDARD 90
    C_STANDARD_REQUIRED on
    C_EXTENSIONS on
    C_VISIBILITY_PRESET hidden
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED on
    CXX_EXTENSIONS on
    POSITION_INDEPENDENT_CODE true
    INTERPROCEDURAL_OPTIMIZATION true
)

target_sources(ntt_unit_tests
  PRIVATE
    ${NTT_SOURCES}
    ${NTT_TEST_SOURCES}
)

target_include_directories(ntt_unit_tests
  PUBLIC 
    ./include
)

find_package(Threads REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(jemalloc REQUIRED IMPORTED_TARGET jemalloc)

target_link_libraries(
  ntt_unit_tests
  PRIVATE 
    Boost::headers 
    GTest::gtest
    GTest::gtest_main 
    Threads::Threads 
    PkgConfig::jemalloc
)

add_test(NAME ntt_unit_tests COMMAND ntt_unit_tests)
