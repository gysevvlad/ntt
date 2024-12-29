cmake_minimum_required(VERSION 3.23)

project(ntt)

add_subdirectory(thd SYSTEM)


#[[ base obj library ]]#

add_library(ntt OBJECT)

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

# ntt sources
target_sources(ntt
  PRIVATE
    src/accept_source.c
    src/connect_request.c
    src/cstr.c
    src/event_source.c
    src/impl/sockaddr_in.c
    src/impl/sockaddr_in6.c
    src/impl/sockaddr_un.c
    src/in6_addr.c
    src/in_addr.c
    src/pool.c
    src/queue.c
    src/sockaddr.c
    src/task.c
    src/task_cache.c
    src/tcp_source.c
    src/thread.c
    src/util.c
    src/view.c
  PRIVATE
    FILE_SET ntt_impl_headers
    TYPE HEADERS
    FILES
      include/ntt/impl/connect_request.h
      include/ntt/impl/event_source.h
      include/ntt/impl/in6_addr.h
      include/ntt/impl/in_addr.h
      include/ntt/impl/sockaddr_in.h
      include/ntt/impl/sockaddr_in6.h
      include/ntt/impl/sockaddr_un.h
      include/ntt/impl/table.h
      include/ntt/impl/tcp_source.h
    BASE_DIRS
      include
  PUBLIC
    FILE_SET ntt_public_headers
    TYPE HEADERS
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/include/ntt/export.h
      include/ntt/accept_source.h
      include/ntt/connect_request.h
      include/ntt/cstr.h
      include/ntt/defs.h
      include/ntt/event_source.h
      include/ntt/pool.h
      include/ntt/sockaddr.h
      include/ntt/task.h
      include/ntt/task_queue.h
      include/ntt/tcp_source.h
      include/ntt/util.h
      include/ntt/view.h
    BASE_DIRS
      ${CMAKE_CURRENT_BINARY_DIR}/include
      include)

# links

# TODO: cmake dependency management
include(FindPkgConfig)
find_package(Threads REQUIRED)
target_link_libraries(ntt PUBLIC PRIVATE Threads::Threads)


#[[ static ntt lib ]]#

add_library(ntt_static STATIC)
set_target_properties(ntt_static PROPERTIES OUTPUT_NAME "ntt")
target_link_libraries(ntt_static PUBLIC ntt)


#[[ shared ntt lib ]]#

add_library(ntt_shared SHARED)
set_target_properties(ntt_shared PROPERTIES OUTPUT_NAME "ntt")
target_link_libraries(ntt_shared PUBLIC ntt)


#[[ internal tests ]]#

# add_executable(ntt_internal_tests
#   src/queue_test.c)

# target_link_libraries(ntt_internal_tests ntt_static ntt::thd::criterion)

# add_test(NAME internal_tests COMMAND ntt_internal_tests)


#[[ unit tests ]]#

add_executable(ntt_unit_tests
  tests/AcceptSourceTest.cpp
  tests/Pool2Test.cpp
  tests/PoolTest.cpp
  tests/SockaddrTest.cpp
  tests/TaskTest.cpp
  tests/ThreadTest.cpp
  tests/UtilTest.cpp
)

set_target_properties(
  ntt_unit_tests
  PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED on
    CXX_EXTENSIONS on)

# TODO: cmake dependency management
find_package(GTest)
find_package(Boost)
pkg_search_module(jemalloc REQUIRED IMPORTED_TARGET jemalloc)
target_link_libraries(ntt_unit_tests PkgConfig::jemalloc ntt_shared Boost::headers GTest::gtest_main GTest::gtest)
target_link_libraries(ntt_unit_tests ntt_static Boost::headers GTest::gtest_main GTest::gtest)

add_test(NAME unit_tests COMMAND ntt_unit_tests)

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
