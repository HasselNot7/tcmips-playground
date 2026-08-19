cmake_minimum_required(VERSION 4.3)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchains/cmake/tcmips.toolchain.cmake")
project(tcmips_c)

set(CMAKE_CXX_STANDARD 17)

add_executable(tcmips_c main.cpp)
add_executable(tcmips_snake src/snake.cpp)
add_executable(tcmips_breakout src/breakout.cpp)
add_executable(tcmips_guess src/guess.cpp)
add_executable(tcmips_reaction src/reaction.cpp)
add_executable(tcmips_clock src/clock.cpp)
add_executable(tcmips_life src/life.cpp)
