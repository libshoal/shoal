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

static uint8_t lua_settings_loaded = 0;

#ifdef BARRELFISH
static const char* SETTINGS_FILE = "shl__settings.lua";
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

    return ok;

}

int shl__get_global_conf(const char *table, const char *field, int def)
{
    int retval = 0;

    if (!lua_settings_loaded) {
        return def;
    }

    lua_timer.start();

    /* load the settings table */
    lua_getglobal(L, table);
    if (!lua_istable(L, -1)) {
        printf("error: settings is not a table\n");
        return def;
    }

    /* load the field */
    lua_pushstring(L, field);
    lua_gettable(L, -2);

    if (!lua_isnumber(L, -1)) {
        printf("error: settings.%s.%s is not a number\n", table, field);
    } else {
        retval = (int)lua_tonumber(L, -1);
    }

    /* pop the pusehd values */
    lua_pop(L, 1);

    lua_timer.stop();

    return retval;
}



#if 0
/**
 * \brief returns a global setting parameter
 *
 * \param setting the setting to obtain
 *
 * \returns
 */
int shl__get_global_conf(const char *setting, int def)
{
    int retval = def;

    if (!lua_settings_loaded) {
        return def;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "evalExpr=%s", setting);

    lua_timer.start();

    if (!luaL_dostring(L, buf)) {

        /* Get the value of the global varibable */

        lua_getglobal(L, "evalExpr");

        if (lua_isnumber(L, -1)) {
            retval = lua_tonumber(L, -1);
        }

        /* remove lua_getglobal value */
        lua_pop(L, 1);
    }

    lua_timer.stop();

    return retval;
}
#endif

/**
 * \brief Return array feature configuration for given array
 */
bool shl__get_array_conf(const char *array_name, int feature, bool def)
{
    if (!lua_settings_loaded) {
        return 1;
    }
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
    if ( luaL_dofile( L, SETTINGS_FILE ) == 0) {
        lua_settings_loaded = 1;
        printf("Lua settings loaded successfully %s\n", SETTINGS_FILE);
    } else {
        printf("Error loading %s\n", SETTINGS_FILE);
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
