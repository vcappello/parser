cmake_minimum_required(VERSION 3.5.0)
project(parser VERSION 0.1.0 LANGUAGES C CXX)

add_executable(parser main.cpp)

set_property(TARGET parser PROPERTY CXX_STANDARD 17)

include(CTest)
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(tests tests/test_base.cpp)
target_link_libraries(tests GTest::GTest GTest::Main)
gtest_discover_tests(tests)
