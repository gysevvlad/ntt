# Google Benchmark, fetched at configure time.

if (NOT NTT_BUILD_BENCHMARKS)
  return()
endif()

include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF)
set(BENCHMARK_ENABLE_INSTALL OFF)

FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.4
  GIT_SHALLOW true)

FetchContent_MakeAvailable(googlebenchmark)
