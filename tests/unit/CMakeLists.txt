cmake_minimum_required(VERSION 3.14)
project(note-emu-tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-function)

set(NOTE_EMU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(NOTE_EMU_SRC "${NOTE_EMU_ROOT}/src")

add_executable(note-emu-tests
    test_emu.cpp
    ${NOTE_EMU_SRC}/note/emu/emu.c
)

target_include_directories(note-emu-tests PRIVATE
    ${NOTE_EMU_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME unit-tests COMMAND note-emu-tests)
