cmake_minimum_required(VERSION 3.22)

project(ntt)

#[[ base obj library ]]#

add_library(ntt)
add_library(ntt::ntt ALIAS ntt)

# setup properties
set_target_properties(ntt
  PROPERTIES
    C_STANDARD 90
    C_STANDARD_REQUIRED on
    C_EXTENSIONS on
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE true
    INTERPROCEDURAL_OPTIMIZATION true)

# generate export header
include(GenerateExportHeader)
generate_export_header(ntt EXPORT_FILE_NAME include/ntt/export.h)

# find_package(PkgConfig REQUIRED)
# pkg_search_module(CARES REQUIRED IMPORTED_TARGET libcares)

# ntt sources
target_sources(ntt
  PRIVATE
    # src/accept_source.c
    # src/connect_request.c
    # src/context.c
    # src/event_source.c
    # src/pool.c
    # src/queue.c
    # src/selector.c
    # src/session.c
    # src/task_cache.c
    # src/tcp_source.c
    # src/thread.c
    src/cstr.c
    src/error.c
    src/event.c
    src/loop.c
    src/malloc.c
    src/reader.c
    src/signal.c
    src/signal_source.c
    src/sockaddr.c
    src/socket.c
    src/task.c
    src/util.c
    src/view.c
    src/worker.c
  PRIVATE
    # FILE_SET ntt_impl_headers
    # TYPE HEADERS
    # FILES
      include/ntt/impl/connect_request.h
      include/ntt/impl/event_source.h
      include/ntt/impl/in_addr.h
      include/ntt/impl/malloc.h
      include/ntt/impl/selector.h
      include/ntt/impl/session.h
      include/ntt/impl/table.h
      include/ntt/impl/tcp_source.h
      include/ntt/impl/worker.h
    # BASE_DIRS
    #   include
  # PUBLIC
  #   FILE_SET ntt_public_headers
  #   TYPE HEADERS
  #   FILES
      ${CMAKE_CURRENT_BINARY_DIR}/include/ntt/export.h
      include/ntt/accept_source.h
      include/ntt/connect_request.h
      include/ntt/defs.h
      include/ntt/cstr.h
      include/ntt/event_source.h
      include/ntt/reader.h
      include/ntt/session.h
      include/ntt/signal.h
      include/ntt/signal_source.h
      include/ntt/sockaddr.h
      include/ntt/task.h
      include/ntt/task_queue.h
      include/ntt/tcp_source.h
      include/ntt/util.h
      include/ntt/view.h
      include/ntt/worker.h
    # BASE_DIRS
    #   ${CMAKE_CURRENT_BINARY_DIR}/include
    #   include
)

target_include_directories(ntt
  PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR}/include
    include)

# links

find_package(Threads REQUIRED)
target_link_libraries(ntt PRIVATE Threads::Threads)

#[[ unit tests ]]#

add_executable(ntt_unit_tests
  tests/LoopTest.cpp
  tests/SockaddrTest.cpp
  tests/SocketTest.cpp
  tests/TaskTest.cpp
  tests/UtilTest.cpp
  tests/WorkerTest.cpp
)

set_target_properties(
  ntt_unit_tests
  PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED on
    CXX_EXTENSIONS on)

# TODO: cmake dependency management
find_package(GTest REQUIRED)
find_package(Boost REQUIRED)
target_link_libraries(ntt_unit_tests ntt::ntt Boost::headers GTest::gtest_main GTest::gtest)

add_test(NAME unit_tests COMMAND ntt_unit_tests)

add_executable(example-00 example/example-00.c)
target_link_libraries(example-00 PRIVATE ntt::ntt)

add_executable(example-01 example/example-01.c)
target_link_libraries(example-01 PRIVATE ntt::ntt)

add_executable(example-02 example/example-02.c)
target_link_libraries(example-02 PRIVATE ntt::ntt)

add_executable(example-03 example/example-03.c)
target_link_libraries(example-03 PRIVATE ntt::ntt)

# add_executable(example-04 example/example-04.c)
# target_link_libraries(example-04 PRIVATE ntt::ntt)

#[[ ntt package ]]#
# TODO: provide ntt package

# install(
#   TARGETS ntt
#   EXPORT nttTargets
#   FILE_SET ntt_public_headers
#   COMPONENT ntt)

# install(EXPORT nttTargets NAMESPACE ntt:: DESTINATION lib/ntt/cmake)

# include(CMakePackageConfigHelpers)

# configure_package_config_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/config.cmake.in
#   ${CMAKE_CURRENT_BINARY_DIR}/nttConfig.cmake
#   INSTALL_DESTINATION lib/ntt/cmake)

# install(
#   FILES ${CMAKE_CURRENT_BINARY_DIR}/nttConfig.cmake
#   DESTINATION lib/ntt/cmake)

# add_subdirectory(learn)
