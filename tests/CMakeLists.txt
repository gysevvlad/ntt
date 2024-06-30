
find_package(GTest)
find_package(Boost)

add_executable(ntt_tests
    # signal_cv.test.cpp
    loop.cpp
    work_loop.cpp
    thread_pool.cpp
)

pkg_search_module(jemalloc REQUIRED IMPORTED_TARGET jemalloc)

target_link_libraries(ntt_tests PRIVATE PkgConfig::jemalloc ntt Boost::headers GTest::gtest_main GTest::gtest)

set_target_properties(
  ntt_tests
  PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

