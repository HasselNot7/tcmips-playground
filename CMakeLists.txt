cmake_minimum_required(VERSION 4.3)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchains/cmake/tcmips.toolchain.cmake")
project(tcmips_c)

set(CMAKE_CXX_STANDARD 17)

add_executable(tcmips_c main.cpp)
add_executable(tcmips_snake src/games/snake.cpp)
add_executable(tcmips_reaction src/games/reaction.cpp)
add_executable(tcmips_clock src/tools/clock.cpp)
add_executable(tcmips_life src/games/life.cpp)
add_executable(tcmips_xiangqi src/games/xiangqi.cpp)
add_executable(tcmips_wolf3d src/games/wolf3d/wolfmain.cpp src/games/wolf3d/id_ca.cpp src/games/wolf3d/id_in.cpp src/games/wolf3d/id_pm.cpp src/games/wolf3d/id_sd.cpp src/games/wolf3d/id_us_1.cpp src/games/wolf3d/id_vh.cpp src/games/wolf3d/id_vl.cpp src/games/wolf3d/signon.cpp src/games/wolf3d/wl_act1.cpp src/games/wolf3d/wl_act2.cpp src/games/wolf3d/wl_agent.cpp src/games/wolf3d/wl_draw.cpp src/games/wolf3d/wl_game.cpp src/games/wolf3d/wl_inter.cpp src/games/wolf3d/wl_main.cpp src/games/wolf3d/wl_menu.cpp src/games/wolf3d/wl_play.cpp src/games/wolf3d/wl_state.cpp src/games/wolf3d/wl_text.cpp src/games/wolf3d/tcm_files.cpp asset/wolf3d/wolfdata.cpp)
target_include_directories(tcmips_wolf3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/asset/wolf3d)
