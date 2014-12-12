#ifdef BARRELFISH
extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}
#else
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
#endif

#include <cstdio>
#include <cassert>
#include <cstring>

#include "shl_internal.h"
#include "shl_timer.hpp"

/**
 *
 */
const char* shl__arr_feature_table[] = {
    "partitioning",
    "replication",
    "distribution",
    "hugepage"
};

#ifdef BARRELFISH
static const char* SETTINGS_FILE = "/nfs/array_settings.lua";
#else
static const char* SETTINGS_FILE = "settings.lua";
#endif

///<
static lua_State *L = NULL;

///<
static Timer lua_timer;

#if 0
// http://windrealm.org/tutorials/reading-a-lua-configuration-file-from-c.php
static const char* shl__lua_stringexpr(lua_State* lua, const char* expr, const char* def)
{

    const char* r = def;
    char buf[256] = "";

    /* Assign the Lua expression to a Lua global variable. */
    sprintf(buf, "evalExpr=%s", expr);
    if (!luaL_dostring(lua, buf)) {

        /* Get the value of the global varibable */
        lua_getglobal(lua, "evalExpr");
        if (lua_isstring(lua, -1)) {
            r = lua_tostring(lua, -1);
        }

        /* remove lua_getglobal value */
        lua_pop(lua, 1);
    }

    return r;
}
#endif

/**
 *
 * @param lua
 * @param expr
 * @param def_val
 * @return
 */
static int shl__lua_boolexpr(lua_State* lua, const char* expr, bool def_val)
{
    int ok = def_val;
    char buf[256] = "";

    /* Assign the Lua expression to a Lua global variable. */

    printf("Querying [%s] ..", expr);

    sprintf(buf, "evalExpr=%s", expr);

    if (!luaL_dostring(lua, buf)) {

        /* Get the value of the global varibable */

        lua_getglobal(lua, "evalExpr");

        if (lua_isboolean(lua, -1)) {
            ok = lua_toboolean(lua, -1);
            printf(" .. found %d", ok);
        }

        /* remove lua_getglobal value */

        lua_pop(lua, 1);

    }

    printf(" .. result %d\n", ok);

    return ok;

}

/**
 * \brief Return array feature configuration for given array
 */
bool shl__get_array_conf(const char *array_name, int feature, bool def)
{
    lua_timer.start();
    assert(strlen(array_name) < SHL__ARRAY_NAME_LEN_MAX);
    assert(strncmp(array_name, "shl__", 5) == 0);

    array_name = array_name + strlen("shl__");

    char tmp[256 + SHL__ARRAY_NAME_LEN_MAX];

    sprintf(tmp, "settings.arrays[\"%s\"].%s", array_name,
            shl__arr_feature_table[feature]);

    bool res = shl__lua_boolexpr(L, tmp, def);
    lua_timer.stop();

    return res;
}

/**
 *
 */
void shl__lua_init(void)
{
    lua_timer.start();
    L = luaL_newstate();
    if (L) {
        luaL_openlibs(L);
    } else {
        printf("ERROR: opening LUA\n");
    }

    // Load settings file
    if ( luaL_dofile( L, SETTINGS_FILE ) == 1) {
        printf("Error loading %s\n", SETTINGS_FILE);
        assert(!"Error loading settings-file. Check it's syntax.");
    }

    // luaL_loadbuffer(L, program, strlen(program), "line")
    lua_timer.stop();
}

/**
 *
 */
void shl__lua_deinit(void)
{
    lua_close(L);
    L = NULL;
    printf("LUA done (time spent: %f)\n", lua_timer.get());
}
