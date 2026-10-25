# Builds the chess program (chess.c) as strict C99.
# Open this folder in CLion; it reads this file and builds "chess".
cmake_minimum_required(VERSION 3.20)
project(chess_c99 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(chess chess.c)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chess PRIVATE -Wall -Wextra -Wpedantic)
endif()
