cmake_minimum_required(VERSION 3.10)
project(chip8-emu)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(libs/tinyfd)

add_executable(chip8 src/main.c)

target_link_libraries(chip8 SDL2)
target_link_libraries(chip8 ${CMAKE_SOURCE_DIR}/libs/tinyfd/tinyfiledialogs.c)