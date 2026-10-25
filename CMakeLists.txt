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

# Golden transcripts (CLion: Run > All CTests, or "ctest" in the build folder):
# version 5's complete output on fixed input scripts. Every later version
# must print exactly the same for the same input.
enable_testing()
file(GLOB golden_inputs "${CMAKE_SOURCE_DIR}/tests/golden/*.in")
foreach (golden_input ${golden_inputs})
    get_filename_component(golden_name "${golden_input}" NAME_WE)
    add_test(NAME golden_${golden_name}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:chess>
                     -DINPUT=${golden_input}
                     -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/golden/${golden_name}.out
                     -DACTUAL=${CMAKE_BINARY_DIR}/${golden_name}.actual
                     -P ${CMAKE_SOURCE_DIR}/tests/compare_transcript.cmake)
endforeach()
