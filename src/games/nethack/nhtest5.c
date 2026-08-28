/* T5: isolate Lua VM execution hang - fptr / setjmp / lua_call / pcall */
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <dev/console.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define P(s) tcm_ascii_console_write_string(s)
#define PL(s) tcm_ascii_console_write_string(s "\r\n")

static int addone(int x) { return x + 1; }
static int (*fptr)(int) = addone;

static int my_cfunc(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer) 42);
    return 1;
}

int main(void)
{
    char tb[64];
    tcm_ascii_console_init();
    tcm_ascii_console_clear();
    PL("T5 start");

    /* 1. indirect call via function pointer */
    {
        int r = fptr(41);
        snprintf(tb, sizeof tb, "fptr=%d", r);
        P(tb);
    }

    /* 2. setjmp/longjmp roundtrip */
    {
        jmp_buf jb;
        volatile int stage = 0;
        int j = setjmp(jb);
        if (j == 0 && stage == 0) {
            stage = 1;
            longjmp(jb, 7);
        }
        snprintf(tb, sizeof tb, "sj=%d stage=%d", j, stage);
        P(tb);
    }

    /* 3. lua state + base lib */
    {
        lua_State *L = luaL_newstate();
        if (!L) {
            PL("newstate=NULL");
            goto idle;
        }
        PL("newstate=ok");
        luaL_requiref(L, "_G", luaopen_base, 1);
        luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
        luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
        lua_pop(L, 2);
        PL("base=ok");

        /* 4. unprotected call of a C closure (OP_CALL -> C function) */
        lua_pushcfunction(L, my_cfunc);
        lua_call(L, 0, 1);
        snprintf(tb, sizeof tb, "ccall=%d", (int) lua_tointeger(L, -1));
        P(tb);
        lua_pop(L, 1);

        /* 5. unprotected call of a Lua chunk (full VM dispatch loop) */
        if (luaL_loadstring(L, "return 1+1") != LUA_OK) {
            PL("load=fail");
            goto idle;
        }
        PL("load=ok");
        lua_call(L, 0, 1);
        snprintf(tb, sizeof tb, "call=%d", (int) lua_tointeger(L, -1));
        P(tb);
        lua_pop(L, 1);

        /* 6. protected call (setjmp path inside ldo.c) */
        if (luaL_loadstring(L, "return 2+2") != LUA_OK) {
            PL("load2=fail");
            goto idle;
        }
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            PL("pcall=fail");
            goto idle;
        }
        PL("pcall=ok");

        /* 7. pure table loop (OP_FORLOOP / OP_NEWTABLE / #) */
        if (luaL_loadstring(L,
                "local t={} for i=1,10 do t[i]=i end return #t") != LUA_OK) {
            PL("load3=fail");
            goto idle;
        }
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            {
                const char *em = lua_tostring(L, -1);
                snprintf(tb, sizeof tb, "pcall3=fail: %s", em ? em : "?");
                P(tb);
            }
            goto idle;
        }
        snprintf(tb, sizeof tb, "loop=%d", (int) lua_tointeger(L, -1));
        P(tb);
        lua_pop(L, 1);

        /* 8. string.format('%d', 42) - isolate lstrlib */
        if (luaL_loadstring(L, "return string.format('x=%d', 42)") != LUA_OK) {
            PL("load4=fail");
            goto idle;
        }
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            const char *em = lua_tostring(L, -1);
            const char *d = strchr(em ? em : "", ':');
            snprintf(tb, sizeof tb, "fmtfail: %s",
                     (em && d && d[1]) ? d + 2 : (em ? em : "?"));
            P(tb);
            goto idle;
        }
        {
            const char *r = lua_tostring(L, -1);
            snprintf(tb, sizeof tb, "fmt=%s", r ? r : "?");
            P(tb);
        }
    }

    PL("T5 done");
idle:
    for (;;) {
        volatile unsigned char *v = (volatile unsigned char *) 0x03C00000;
        for (volatile int d = 0; d < 300000; d++) {}
        v[0] = v[0] ? 0 : 0xFF;
    }
}
