# Runs the chess program on one input script and compares its whole output
# with the golden transcript recorded from version 5. Windows line endings
# are ignored; everything else must be identical, byte for byte.
# Used by CTest: cmake -DEXE=... -DINPUT=... -DEXPECTED=... -DACTUAL=... -P compare_transcript.cmake
execute_process(COMMAND "${EXE}"
                INPUT_FILE "${INPUT}"
                OUTPUT_VARIABLE actual
                ERROR_VARIABLE errors
                RESULT_VARIABLE result)
file(READ "${EXPECTED}" expected)
string(REPLACE "\r" "" actual "${actual}")
string(REPLACE "\r" "" expected "${expected}")
if (NOT result EQUAL 0)
    message(FATAL_ERROR "the program exited with ${result}: ${errors}")
endif()
if (NOT actual STREQUAL expected)
    file(WRITE "${ACTUAL}" "${actual}")
    message(FATAL_ERROR "output differs from ${EXPECTED}; the actual output is in ${ACTUAL}")
endif()
