cmake_minimum_required(VERSION 3.22)

project(ntt)

option(NTT_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(NTT_BUILD_BENCHMARKS "Build benchmarks" OFF)

set(NTT_SOURCES
  src/combine.c
  src/log.c
  src/loop.c
  src/malloc.c
  src/queue.c
  src/task.c
  src/timer.c
  src/timer_heap.c
  src/timer_service.c
  src/queue_heap.c
  src/queue_slab.c
  src/worker.c
  src/signal.c
  src/die.c
  src/event.c
)

set(NTT_TEST_SOURCES
  src/combine_test.cpp
  src/list_test.cpp
  src/loop_test.cpp
  src/queue_test.cpp
  src/task_list_test.cpp
  src/task_test.cpp
  src/timer_heap_test.cpp
  src/timer_test.cpp
  src/worker_test.cpp
)

set(NTT_BENCHMARK_SOURCES
  src/queue_benchmark.cpp
  src/timer_heap_benchmark.cpp
  src/timer_loop_benchmark.cpp
)

find_package(Threads REQUIRED)

if (NTT_BUILD_TESTS)
  enable_testing()

  add_executable(ntt_unit_tests)

  set_target_properties(ntt_unit_tests
    PROPERTIES
      C_STANDARD 17
      C_STANDARD_REQUIRED on
      C_EXTENSIONS on
      CXX_STANDARD 23
      CXX_STANDARD_REQUIRED on
      CXX_EXTENSIONS on
      INTERPROCEDURAL_OPTIMIZATION true)

  target_sources(ntt_unit_tests PRIVATE ${NTT_SOURCES} ${NTT_TEST_SOURCES})

  target_include_directories(ntt_unit_tests PUBLIC ./include)

  find_package(GTest REQUIRED)
  find_package(Boost REQUIRED)

  target_link_libraries(
    ntt_unit_tests
    PRIVATE 
      Boost::headers 
      GTest::gtest
      GTest::gtest_main 
      Threads::Threads)

  add_test(NAME ntt_unit_tests COMMAND ntt_unit_tests)
endif()

add_subdirectory(thd)

if (NTT_BUILD_BENCHMARKS)
  add_executable(ntt_benchmarks)

  set_target_properties(ntt_benchmarks
    PROPERTIES
      C_STANDARD 17
      C_STANDARD_REQUIRED on
      C_EXTENSIONS on
      CXX_STANDARD 23
      CXX_STANDARD_REQUIRED on
      CXX_EXTENSIONS on
      INTERPROCEDURAL_OPTIMIZATION true)

  target_sources(ntt_benchmarks PRIVATE ${NTT_SOURCES} ${NTT_BENCHMARK_SOURCES})

  target_include_directories(ntt_benchmarks PUBLIC ./include)

  find_package(Boost REQUIRED)

  target_link_libraries(
    ntt_benchmarks
    PRIVATE
      Boost::headers
      benchmark::benchmark_main
      Threads::Threads)
endif()

add_library(ntt SHARED)
add_library(ntt::ntt ALIAS ntt)

target_include_directories(ntt PUBLIC include)
target_sources(ntt PRIVATE ${NTT_SOURCES})
set_target_properties(ntt
  PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED on
    C_EXTENSIONS on
    C_VISIBILITY_PRESET hidden
    INTERPROCEDURAL_OPTIMIZATION true
)

target_link_libraries(ntt PRIVATE Threads::Threads)

add_executable(ntt_example_00 example/example-00.c)

target_link_libraries(ntt_example_00 PRIVATE ntt::ntt)
