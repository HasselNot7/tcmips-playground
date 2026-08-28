cmake_minimum_required(VERSION 4.3)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchains/cmake/tcmips.toolchain.cmake")
project(tcmips_c)

set(CMAKE_CXX_STANDARD 17)

add_executable(tcmips_snake src/games/snake.cpp)
add_executable(tcmips_reaction src/games/reaction.cpp)
add_executable(tcmips_clock src/tools/clock.cpp)
add_executable(tcmips_life src/games/life.cpp)
add_executable(tcmips_xiangqi src/games/xiangqi.cpp)
add_executable(tcmips_wolf3d src/games/wolf3d/wolfmain.cpp src/games/wolf3d/id_ca.cpp src/games/wolf3d/id_in.cpp src/games/wolf3d/id_pm.cpp src/games/wolf3d/id_sd.cpp src/games/wolf3d/id_us_1.cpp src/games/wolf3d/id_vh.cpp src/games/wolf3d/id_vl.cpp src/games/wolf3d/signon.cpp src/games/wolf3d/wl_act1.cpp src/games/wolf3d/wl_act2.cpp src/games/wolf3d/wl_agent.cpp src/games/wolf3d/wl_draw.cpp src/games/wolf3d/wl_game.cpp src/games/wolf3d/wl_inter.cpp src/games/wolf3d/wl_main.cpp src/games/wolf3d/wl_menu.cpp src/games/wolf3d/wl_play.cpp src/games/wolf3d/wl_state.cpp src/games/wolf3d/wl_text.cpp src/games/wolf3d/tcm_files.cpp asset/wolf3d/wolfdata.cpp)
target_include_directories(tcmips_wolf3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/asset/wolf3d)
add_executable(tcmips_nhprobe src/games/nethack/nhprobe.c)
target_compile_definitions(tcmips_nhprobe PRIVATE TCMIPS_PORT)

add_executable(tcmips_nethack
  src/games/nethack/core/allmain.c
  src/games/nethack/core/alloc.c
  src/games/nethack/core/apply.c
  src/games/nethack/core/artifact.c
  src/games/nethack/core/attrib.c
  src/games/nethack/core/ball.c
  src/games/nethack/core/bones.c
  src/games/nethack/core/botl.c
  src/games/nethack/core/calendar.c
  src/games/nethack/core/cfgfiles.c
  src/games/nethack/core/cmd.c
  src/games/nethack/core/coloratt.c
  src/games/nethack/core/date.c
  src/games/nethack/core/dbridge.c
  src/games/nethack/core/decl.c
  src/games/nethack/core/detect.c
  src/games/nethack/core/dig.c
  src/games/nethack/core/display.c
  src/games/nethack/core/dlb.c
  src/games/nethack/core/do.c
  src/games/nethack/core/do_name.c
  src/games/nethack/core/do_wear.c
  src/games/nethack/core/dog.c
  src/games/nethack/core/dogmove.c
  src/games/nethack/core/dokick.c
  src/games/nethack/core/dothrow.c
  src/games/nethack/core/drawing.c
  src/games/nethack/core/dungeon.c
  src/games/nethack/core/earlyarg.c
  src/games/nethack/core/eat.c
  src/games/nethack/core/end.c
  src/games/nethack/core/engrave.c
  src/games/nethack/core/exper.c
  src/games/nethack/core/explode.c
  src/games/nethack/core/extralev.c
  src/games/nethack/core/files.c
  src/games/nethack/core/fountain.c
  src/games/nethack/core/getpos.c
  src/games/nethack/core/glyphs.c
  src/games/nethack/core/hack.c
  src/games/nethack/core/hacklib.c
  src/games/nethack/core/iactions.c
  src/games/nethack/core/insight.c
  src/games/nethack/core/invent.c
  src/games/nethack/core/isaac64.c
  src/games/nethack/core/light.c
  src/games/nethack/core/lock.c
  src/games/nethack/core/mail.c
  src/games/nethack/core/makemon.c
  src/games/nethack/core/mcastu.c
  src/games/nethack/core/mdlib.c
  src/games/nethack/core/mhitm.c
  src/games/nethack/core/mhitu.c
  src/games/nethack/core/minion.c
  src/games/nethack/core/mklev.c
  src/games/nethack/core/mkmap.c
  src/games/nethack/core/mkmaze.c
  src/games/nethack/core/mkobj.c
  src/games/nethack/core/mkroom.c
  src/games/nethack/core/mon.c
  src/games/nethack/core/mondata.c
  src/games/nethack/core/monmove.c
  src/games/nethack/core/monst.c
  src/games/nethack/core/mplayer.c
  src/games/nethack/core/mthrowu.c
  src/games/nethack/core/muse.c
  src/games/nethack/core/music.c
  src/games/nethack/core/nhlobj.c
  src/games/nethack/core/nhlsel.c
  src/games/nethack/core/nhlua.c
  src/games/nethack/core/nhmd4.c
  src/games/nethack/core/o_init.c
  src/games/nethack/core/objects.c
  src/games/nethack/core/objnam.c
  src/games/nethack/core/options.c
  src/games/nethack/core/pager.c
  src/games/nethack/core/pickup.c
  src/games/nethack/core/pline.c
  src/games/nethack/core/polyself.c
  src/games/nethack/core/potion.c
  src/games/nethack/core/pray.c
  src/games/nethack/core/priest.c
  src/games/nethack/core/quest.c
  src/games/nethack/core/questpgr.c
  src/games/nethack/core/read.c
  src/games/nethack/core/rect.c
  src/games/nethack/core/region.c
  src/games/nethack/core/report.c
  src/games/nethack/core/restore.c
  src/games/nethack/core/rip.c
  src/games/nethack/core/rnd.c
  src/games/nethack/core/role.c
  src/games/nethack/core/rumors.c
  src/games/nethack/core/save.c
  src/games/nethack/core/selvar.c
  src/games/nethack/core/sfbase.c
  src/games/nethack/core/sfstruct.c
  src/games/nethack/core/shk.c
  src/games/nethack/core/shknam.c
  src/games/nethack/core/sit.c
  src/games/nethack/core/sounds.c
  src/games/nethack/core/sp_lev.c
  src/games/nethack/core/spell.c
  src/games/nethack/core/stairs.c
  src/games/nethack/core/steal.c
  src/games/nethack/core/steed.c
  src/games/nethack/core/strutil.c
  src/games/nethack/core/symbols.c
  src/games/nethack/core/sys.c
  src/games/nethack/core/teleport.c
  src/games/nethack/core/timeout.c
  src/games/nethack/core/topten.c
  src/games/nethack/core/track.c
  src/games/nethack/core/trap.c
  src/games/nethack/core/u_init.c
  src/games/nethack/core/uhitm.c
  src/games/nethack/core/utf8map.c
  src/games/nethack/core/vault.c
  src/games/nethack/core/version.c
  src/games/nethack/core/vision.c
  src/games/nethack/core/weapon.c
  src/games/nethack/core/were.c
  src/games/nethack/core/wield.c
  src/games/nethack/core/windows.c
  src/games/nethack/core/wizard.c
  src/games/nethack/core/wizcmds.c
  src/games/nethack/core/worm.c
  src/games/nethack/core/worn.c
  src/games/nethack/core/write.c
  src/games/nethack/core/zap.c
  src/games/nethack/tty/getline.c
  src/games/nethack/tty/topl.c
  src/games/nethack/tty/wintty.c
  src/games/nethack/sys/random.c
  src/games/nethack/lua/lapi.c
  src/games/nethack/lua/lauxlib.c
  src/games/nethack/lua/lbaselib.c
  src/games/nethack/lua/lcode.c
  src/games/nethack/lua/lcorolib.c
  src/games/nethack/lua/lctype.c
  src/games/nethack/lua/ldblib.c
  src/games/nethack/lua/ldebug.c
  src/games/nethack/lua/ldo.c
  src/games/nethack/lua/ldump.c
  src/games/nethack/lua/lfunc.c
  src/games/nethack/lua/lgc.c
  src/games/nethack/lua/linit.c
  src/games/nethack/lua/liolib.c
  src/games/nethack/lua/llex.c
  src/games/nethack/lua/lmathlib.c
  src/games/nethack/lua/lmem.c
  src/games/nethack/lua/loadlib.c
  src/games/nethack/lua/lobject.c
  src/games/nethack/lua/lopcodes.c
  src/games/nethack/lua/loslib.c
  src/games/nethack/lua/lparser.c
  src/games/nethack/lua/lstate.c
  src/games/nethack/lua/lstring.c
  src/games/nethack/lua/lstrlib.c
  src/games/nethack/lua/ltable.c
  src/games/nethack/lua/ltablib.c
  src/games/nethack/lua/ltm.c
  src/games/nethack/lua/lundump.c
  src/games/nethack/lua/lutf8lib.c
  src/games/nethack/lua/lvm.c
  src/games/nethack/lua/lzio.c
  src/games/nethack/tcm_main.c
  src/games/nethack/tcm_vterm.c
  src/games/nethack/tcm_termcap.c
  src/games/nethack/tcm_fs.c
  src/games/nethack/tcm_kbd.c
  src/games/nethack/tcm_dirent.c
  src/games/nethack/nhfont.c
  src/games/nethack/tcm_hooks.c
  src/games/nethack/sys/posixregex.c
  asset/nethack/nhdata.cpp
)
target_include_directories(tcmips_nethack PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/games/nethack/include ${CMAKE_CURRENT_SOURCE_DIR}/src/games/nethack/lua ${CMAKE_CURRENT_SOURCE_DIR}/src/games/nethack ${CMAKE_CURRENT_SOURCE_DIR}/asset/nethack)
target_compile_definitions(tcmips_nethack PRIVATE TCMIPS_PORT GCC_UMINUS_NOSTATICUNUSED NO_SIGNAL NOMAIL NO_TERMCAP_HEADERS)
target_compile_options(tcmips_nethack PRIVATE -fsigned-char -fno-omit-frame-pointer -fno-jump-tables)

add_executable(tcmips_nhtest src/games/nethack/nhtest.c)
target_compile_definitions(tcmips_nhtest PRIVATE TCMIPS_PORT)

add_executable(tcmips_nhtest2 src/games/nethack/nhtest2.c src/games/nethack/tcm_vterm.c src/games/nethack/nhfont.c src/games/nethack/tcm_kbd.c )
target_compile_definitions(tcmips_nhtest2 PRIVATE TCMIPS_PORT)
target_include_directories(tcmips_nhtest2 PRIVATE src/games/nethack src/games/nethack/include)

add_executable(tcmips_nhtest3 src/games/nethack/nhtest3.c src/games/nethack/tcm_fs.c src/games/nethack/tcm_hooks.c src/games/nethack/tcm_kbd.c asset/nethack/nhdata.cpp)
target_compile_definitions(tcmips_nhtest3 PRIVATE TCMIPS_PORT)
target_include_directories(tcmips_nhtest3 PRIVATE src/games/nethack src/games/nethack/include src/games/nethack/lua asset/nethack)

add_executable(tcmips_nhtest4 src/games/nethack/nhtest4.c src/games/nethack/lua/lapi.c src/games/nethack/lua/lauxlib.c src/games/nethack/lua/lbaselib.c src/games/nethack/lua/lcode.c src/games/nethack/lua/lcorolib.c src/games/nethack/lua/lctype.c src/games/nethack/lua/ldblib.c src/games/nethack/lua/ldebug.c src/games/nethack/lua/ldo.c src/games/nethack/lua/ldump.c src/games/nethack/lua/lfunc.c src/games/nethack/lua/lgc.c src/games/nethack/lua/linit.c src/games/nethack/lua/liolib.c src/games/nethack/lua/llex.c src/games/nethack/lua/lmathlib.c src/games/nethack/lua/lmem.c src/games/nethack/lua/loadlib.c src/games/nethack/lua/lobject.c src/games/nethack/lua/lopcodes.c src/games/nethack/lua/loslib.c src/games/nethack/lua/lparser.c src/games/nethack/lua/lstate.c src/games/nethack/lua/lstring.c src/games/nethack/lua/lstrlib.c src/games/nethack/lua/ltable.c src/games/nethack/lua/ltablib.c src/games/nethack/lua/ltm.c src/games/nethack/lua/lundump.c src/games/nethack/lua/lutf8lib.c src/games/nethack/lua/lvm.c src/games/nethack/lua/lzio.c src/games/nethack/tcm_fs.c src/games/nethack/tcm_hooks.c src/games/nethack/tcm_kbd.c asset/nethack/nhdata.cpp)
target_compile_definitions(tcmips_nhtest4 PRIVATE TCMIPS_PORT)
target_include_directories(tcmips_nhtest4 PRIVATE src/games/nethack/lua src/games/nethack src/games/nethack/include asset/nethack)

set_source_files_properties(src/games/nethack/lua/lvm.c PROPERTIES
  COMPILE_OPTIONS "-O1")

add_executable(tcmips_nhtest5 src/games/nethack/nhtest5.c src/games/nethack/tcm_hooks.c src/games/nethack/lua/lapi.c src/games/nethack/lua/lauxlib.c src/games/nethack/lua/lbaselib.c src/games/nethack/lua/lcode.c src/games/nethack/lua/lcorolib.c src/games/nethack/lua/lctype.c src/games/nethack/lua/ldblib.c src/games/nethack/lua/ldebug.c src/games/nethack/lua/ldo.c src/games/nethack/lua/ldump.c src/games/nethack/lua/lfunc.c src/games/nethack/lua/lgc.c src/games/nethack/lua/linit.c src/games/nethack/lua/liolib.c src/games/nethack/lua/llex.c src/games/nethack/lua/lmathlib.c src/games/nethack/lua/lmem.c src/games/nethack/lua/loadlib.c src/games/nethack/lua/lobject.c src/games/nethack/lua/lopcodes.c src/games/nethack/lua/loslib.c src/games/nethack/lua/lparser.c src/games/nethack/lua/lstate.c src/games/nethack/lua/lstring.c src/games/nethack/lua/lstrlib.c src/games/nethack/lua/ltable.c src/games/nethack/lua/ltablib.c src/games/nethack/lua/ltm.c src/games/nethack/lua/lundump.c src/games/nethack/lua/lutf8lib.c src/games/nethack/lua/lvm.c src/games/nethack/lua/lzio.c)
target_compile_definitions(tcmips_nhtest5 PRIVATE TCMIPS_PORT)
target_include_directories(tcmips_nhtest5 PRIVATE src/games/nethack/lua src/games/nethack src/games/nethack/include asset/nethack)

add_executable(tcmips_nhtest6 src/games/nethack/nhtest6.c)
target_compile_definitions(tcmips_nhtest6 PRIVATE TCMIPS_PORT)
target_include_directories(tcmips_nhtest6 PRIVATE src/games/nethack)

add_executable(tcmips_nhtest7 src/games/nethack/nhtest7.c src/games/nethack/tcm_fs.c src/games/nethack/tcm_kbd.c src/games/nethack/tcm_hooks.c src/games/nethack/tcm_vterm.c src/games/nethack/nhfont.c asset/nethack/nhdata.cpp)
target_compile_definitions(tcmips_nhtest7 PRIVATE TCMIPS_PORT)
target_include_directories(tcmips_nhtest7 PRIVATE src/games/nethack src/games/nethack/include src/games/nethack/lua asset/nethack)
