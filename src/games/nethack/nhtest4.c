/* Minimal Lua engine test on device */
#include <stdio.h>
#include <dev/console.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

int main(void) {
    tcm_ascii_console_init();
    tcm_ascii_console_clear();
    tcm_ascii_console_write_string("T4 lua test\r\n");

    lua_State *L = luaL_newstate();
    if (!L) {
        tcm_ascii_console_write_string("newstate=NULL\r\n");
        goto idle;
    }
    tcm_ascii_console_write_string("newstate=ok\r\n");
    /* openlibs one by one to find the hang */
    tcm_ascii_console_write_string("lib base\r\n");
    luaL_requiref(L, "_G", luaopen_base, 1);
    tcm_ascii_console_write_string("lib table\r\n");
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    tcm_ascii_console_write_string("lib string\r\n");
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    tcm_ascii_console_write_string("lib math\r\n");
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    tcm_ascii_console_write_string("lib utf8\r\n");
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    tcm_ascii_console_write_string("lib coroutine\r\n");
    luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
    tcm_ascii_console_write_string("lib os\r\n");
    luaL_requiref(L, LUA_OSLIBNAME, luaopen_os, 1);
    tcm_ascii_console_write_string("lib io\r\n");
    luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1);
    tcm_ascii_console_write_string("lib bit? none\r\n");
    lua_pop(L, 8);
    tcm_ascii_console_write_string("openlibs=ok\r\n");
    {
        int rc = luaL_dostring(L, "return 1+1");
        tcm_ascii_console_write_string("dostring ok\r\n");
    }
 idle:
    tcm_ascii_console_write_string("T4 done\r\n");
    for (;;) {
        volatile unsigned char *v = (volatile unsigned char *) 0x03C00000;
        for (volatile int d = 0; d < 300000; d++) {}
        v[0] = v[0] ? 0 : 0xFF;
    }
    return 0;
}
