cmake_minimum_required(VERSION 4.3)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchains/cmake/tcmips.toolchain.cmake")
project(tcmips_c)

set(CMAKE_CXX_STANDARD 17)

add_executable(tcmips_c main.cpp)
add_executable(tcmips_snake src/games/snake.cpp)
add_executable(tcmips_breakout src/games/breakout.cpp)
add_executable(tcmips_guess src/games/guess.cpp)
add_executable(tcmips_reaction src/games/reaction.cpp)
add_executable(tcmips_clock src/tools/clock.cpp)
add_executable(tcmips_life src/games/life.cpp)
add_executable(tcmips_xiangqi src/games/xiangqi.cpp)
add_executable(tcmips_atari src/tools/atari.cpp src/emulators/atari_2600.cpp)
