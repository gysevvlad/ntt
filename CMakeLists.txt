cmake_minimum_required(VERSION 3.23)

project(ntt)

include(FindPkgConfig)
include(GenerateExportHeader)

add_library(ntt SHARED)

generate_export_header(ntt EXPORT_FILE_NAME include/ntt/export.h)

target_sources(
  ntt
  PRIVATE
    # src/cqueue.c
    # src/engine.c
    src/loop.c
    src/malloc.c
    src/mpsc_queue.c
    src/signal.c
    src/signal_cv.c
    src/squeue.c
    src/task.c
    src/task_queue.c
    src/thread_pool.c
    src/work_loop.c
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS include ${CMAKE_CURRENT_BINARY_DIR}/include
    FILES
      # include/ntt/cqueue.h
      ${CMAKE_CURRENT_BINARY_DIR}/include/ntt/export.h
      include/ntt/loop.h
      include/ntt/malloc.h
      include/ntt/work_loop.h
      include/ntt/mpsc_queue.h
      # include/ntt/pollable_queue.h
      include/ntt/signal.h
      # include/ntt/signal_cv.h
      include/ntt/squeue.h
      include/ntt/task.h
      include/ntt/task_queue.h
      include/ntt/thread_pool.h)

pkg_search_module(liburcu-cds REQUIRED IMPORTED_TARGET liburcu-cds)
target_link_libraries(ntt PUBLIC PkgConfig::liburcu-cds)
find_package(Threads REQUIRED)
target_link_libraries(ntt PUBLIC Threads::Threads)

set_target_properties(
  ntt
  PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden)

add_subdirectory(tests)
