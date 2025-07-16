cmake_minimum_required(VERSION 3.10)

# Project name and C++ standard
project(FastBook LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
add_executable(fastbook
    src/main.cpp
    src/order.cpp
)

# Enable extra warnings
target_compile_options(fastbook PRIVATE -Wall -Wextra -O2)