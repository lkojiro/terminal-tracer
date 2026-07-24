cmake_minimum_required(VERSION 3.16)
project(terminal_tracer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Turn on decent warnings — catching bugs at compile time saves you
# hours later, especially with matrix/vector math where a swapped
# index compiles fine and just gives you garbage output.
add_compile_options(-Wall -Wextra -O2)

add_executable(rasterizer
    src/main.cpp
)

target_include_directories(rasterizer PRIVATE include)

# --- Tests ---
# Zero-dependency unit tests (see include/test_framework.hpp).
enable_testing()

add_executable(run_tests
    tests/test_main.cpp
    tests/test_math.cpp
)

target_include_directories(run_tests PRIVATE include)

# Registers run_tests with ctest, so `ctest --output-on-failure` works
# from the build directory in addition to running the binary directly.
add_test(NAME unit_tests COMMAND run_tests)
