cmake_minimum_required(VERSION 3.10)

# Project name and C++ standard
project(FastBook LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)

# Main executable
add_executable(fastbook src/main.cpp src/ring_buffer.cpp src/order_pool.cpp src/order.cpp src/server.cpp)
target_include_directories(fastbook PRIVATE include)

add_executable(tests
    tests/main_test.cpp
    tests/test_ring_buffer.cpp
    tests/test_order_pool.cpp
    tests/test_order.cpp
    src/ring_buffer.cpp
    src/order_pool.cpp
    src/order.cpp
)
target_link_libraries(tests GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE include)
target_include_directories(tests PRIVATE src)



add_test(NAME UnitTests COMMAND tests)

# Enable extra warnings
target_compile_options(fastbook PRIVATE -Wall -Wextra -O2)