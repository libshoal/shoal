extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

#include <cstdio>
#include <cassert>
#include <cstring>

#include "shl_internal.h"
#include "shl_timer.hpp"

static const char* SETTINGS_FILE = "/home/skaestle/projects/gm/settings.lua" ;

static lua_State *L = NULL;
static Timer lua_timer;

// http://windrealm.org/tutorials/reading-a-lua-configuration-file-from-c.php
const char* shl__lua_stringexpr( lua_State* L, const char* expr,
                            const char* def )
{

    const char* r = def ;
    char buf[256] = "" ;

    /* Assign the Lua expression to a Lua global variable. */
    sprintf( buf, "evalExpr=%s", expr );
    if ( !luaL_dostring( L, buf ) ) {

        /* Get the value of the global varibable */
        lua_getglobal( L, "evalExpr" );
        if ( lua_isstring( L, -1 ) ) {
            r = lua_tostring( L, -1 );
        }

        /* remove lua_getglobal value */
        lua_pop( L, 1 );
    }

    return r;
}

int shl__lua_boolexpr( lua_State* L, const char* expr, bool def_val )
{
    int ok = def_val;
    char buf[256] = "" ;

    /* Assign the Lua expression to a Lua global variable. */

    printf("Querying [%s] ..", expr);

    sprintf( buf, "evalExpr=%s", expr );

    if ( !luaL_dostring( L, buf ) ) {

        /* Get the value of the global varibable */

        lua_getglobal( L, "evalExpr" );

        if ( lua_isboolean( L, -1 ) ) {

            ok = lua_toboolean( L, -1 );
            printf(" .. found %d", ok);

        }

        /* remove lua_getglobal value */

        lua_pop( L, 1 );

    }

    printf(" .. result %d\n", ok);

    return ok ;

}

/**
 * \brief Return array feature configuration for given array
 */
bool shl__get_array_conf(const char *array_name, int feature, bool def)
{
    lua_timer.start();
    assert (strlen(array_name) < SHL__ARRAY_NAME_LEN_MAX);
    assert (strncmp(array_name, "shl__", 5)==0);

    array_name = array_name + strlen("shl__");

    char tmp[256 + SHL__ARRAY_NAME_LEN_MAX];

    sprintf(tmp, "settings.arrays[\"%s\"].%s", array_name,
            shl__arr_feature_table[feature]);

    bool res = shl__lua_boolexpr(L, tmp, def);
    lua_timer.stop();

    return res;
}

void shl__lua_init(void)
{
    lua_timer.start();
    L = luaL_newstate();
     if (L)
        luaL_openlibs(L);

    // Load settings file
    if ( luaL_dofile( L, SETTINGS_FILE ) == 1 ) {

        printf( "Error loading %s\n", SETTINGS_FILE );
        assert (!"Error loading settings-file. Check it's syntax.");
    }

    lua_timer.stop();
}

void shl__lua_deinit(void)
{
    printf("LUA done (time spent: %f)\n", lua_timer.get());
}
