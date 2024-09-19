cmake_minimum_required(VERSION 3.23)

project(ntt)

add_subdirectory(thd SYSTEM)


#[[ base obj library ]]#

add_library(ntt OBJECT)

# setup properties
set_target_properties(ntt
  PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED on
    C_EXTENSIONS off
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE true
    INTERPROCEDURAL_OPTIMIZATION true)

# generate export header
include(GenerateExportHeader)
generate_export_header(ntt EXPORT_FILE_NAME include/ntt/export.h)

# ntt sources
target_sources(ntt
  PRIVATE
    src/task.c
  PUBLIC
    FILE_SET ntt_public_headers
    TYPE HEADERS
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/include/ntt/export.h
      include/ntt/defs.h
      include/ntt/task.h
    BASE_DIRS
      ${CMAKE_CURRENT_BINARY_DIR}/include
      include)

# links

# TODO: cmake dependency management
include(FindPkgConfig)
pkg_search_module(liburcu-cds REQUIRED IMPORTED_TARGET liburcu-cds)
find_package(Threads REQUIRED)
target_link_libraries(ntt PUBLIC PkgConfig::liburcu-cds PRIVATE Threads::Threads)


#[[ static ntt lib ]]#

add_library(ntt_static STATIC)
set_target_properties(ntt_static PROPERTIES OUTPUT_NAME "ntt")
target_link_libraries(ntt_static PUBLIC ntt)


#[[ shared ntt lib ]]#

add_library(ntt_shared SHARED)
set_target_properties(ntt_shared PROPERTIES OUTPUT_NAME "ntt")
target_link_libraries(ntt_shared PUBLIC ntt)


#[[ internal tests ]]#

add_executable(ntt_internal_tests
  src/queue_test.c)

target_link_libraries(ntt_internal_tests ntt_static ntt::thd::criterion)

add_test(NAME internal_tests COMMAND ntt_internal_tests)


#[[ unit tests ]]#

add_executable(ntt_unit_tests
  tests/task.cpp)

set_target_properties(
  ntt_unit_tests
  PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED on
    CXX_EXTENSIONS off)

# TODO: cmake dependency management
find_package(GTest)
find_package(Boost)
pkg_search_module(jemalloc REQUIRED IMPORTED_TARGET jemalloc)
# target_link_libraries(ntt_unit_tests PkgConfig::jemalloc ntt_shared Boost::headers GTest::gtest_main GTest::gtest)
target_link_libraries(ntt_unit_tests ntt_shared Boost::headers GTest::gtest_main GTest::gtest)

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
