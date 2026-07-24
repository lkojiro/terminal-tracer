cmake_minimum_required(VERSION 3.16)
project(terminal_tracer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Turn on decent warnings — catching bugs at compile time saves you
# hours later, especially with matrix/vector math where a swapped
# index compiles fine and just gives you garbage output.
add_compile_options(-Wall -Wextra -O2)

add_executable(rasterizer
    src/main.cpp
)

target_include_directories(rasterizer PRIVATE include)
