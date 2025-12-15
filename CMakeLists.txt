cmake_minimum_required(VERSION 3.10)
project(FastBook LANGUAGES CXX)

# === Build settings ===
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -march=native -fno-omit-frame-pointer")


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
    tests/test_order_book.cpp
    tests/test_order_book_market.cpp
)
target_link_libraries(tests PRIVATE fastbook_lib GTest::GTest GTest::Main)

add_test(NAME UnitTests COMMAND tests)

