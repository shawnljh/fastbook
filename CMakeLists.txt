cmake_minimum_required(VERSION 3.10)
project(FastBook LANGUAGES CXX)

# === Build settings ===
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable warnings
add_compile_options(-Wall -Wextra -Wpedantic -O2)

# === Dependencies ===
enable_testing()
find_package(GTest REQUIRED)

# === Library target ===
# All core source files go into a static library
add_library(fastbook_lib
    src/order.cpp
    src/order_pool.cpp
    src/orderbook.cpp
    src/ring_buffer.cpp
    src/server.cpp
)
target_include_directories(fastbook_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

# === Executables ===
add_executable(fastbook src/main.cpp)
target_link_libraries(fastbook PRIVATE fastbook_lib)

add_executable(tests
    tests/main_test.cpp
    tests/test_order.cpp
    tests/test_order_pool.cpp
    tests/test_ring_buffer.cpp
)
target_link_libraries(tests PRIVATE fastbook_lib GTest::GTest GTest::Main)

add_test(NAME UnitTests COMMAND tests)

