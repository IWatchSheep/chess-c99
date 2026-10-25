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

# Stage 3S tests (CLion: Run > All CTests, or "ctest" in the build folder).
# 1. Unit tests: FEN setup and following pieces across a move.
add_executable(chess_tests tests/chess_tests.c)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chess_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Stage 3A tests: the calculator's report against the worked examples.
add_executable(calc_tests tests/calc_tests.c)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(calc_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Stage 3B tests: the reply view.
add_executable(replies_tests tests/replies_tests.c)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(replies_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME unit_tests COMMAND chess_tests)
add_test(NAME calc_tests COMMAND calc_tests)
add_test(NAME replies_tests COMMAND replies_tests)

# 2. Golden transcripts: complete output on fixed input scripts. tests/golden
#    was recorded from version 5 (the game), tests/golden_v7 from version 7
#    (the R report), tests/golden_v8 from version 8 (the reply view), tests/golden_v9
#    from version 9 (the report after a move that ends the game). Every
#    later version must print exactly the same.
file(GLOB golden_inputs "${CMAKE_SOURCE_DIR}/tests/golden/*.in" "${CMAKE_SOURCE_DIR}/tests/golden_v7/*.in"
                        "${CMAKE_SOURCE_DIR}/tests/golden_v8/*.in" "${CMAKE_SOURCE_DIR}/tests/golden_v9/*.in")
foreach (golden_input ${golden_inputs})
    get_filename_component(golden_name "${golden_input}" NAME_WE)
    get_filename_component(golden_dir "${golden_input}" DIRECTORY)
    add_test(NAME golden_${golden_name}
             COMMAND ${CMAKE_COMMAND}
                     -DEXE=$<TARGET_FILE:chess>
                     -DINPUT=${golden_input}
                     -DEXPECTED=${golden_dir}/${golden_name}.out
                     -DACTUAL=${CMAKE_BINARY_DIR}/${golden_name}.actual
                     -P ${CMAKE_SOURCE_DIR}/tests/compare_transcript.cmake)
endforeach()
