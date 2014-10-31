#include <lua.h>
#include <lauxlib.h>

#include <stdlib.h>
#include <string.h>

/**
 * \brief Based on http://stackoverflow.com/questions/1156572/evaluating-mathematical-expressions-using-lua
 */

static lua_State *L = NULL;

/* Initialize the LE library by creating a Lua state.
 *
 * The new Lua interpreter state has the "usual" standard libraries
 * open.
 */
int le_init()
{
    L = luaL_newstate();
    if (L)
        luaL_openlibs(L);
    return !!L;
}

/* Load an expression, returning a cookie that can be used later to
 * select this expression for evaluation by le_eval(). Note that
 * le_unref() must eventually be called to free the expression.
 *
 * The cookie is a lua_ref() reference to a function that evaluates the
 * expression when called. Any variables in the expression are assumed
 * to refer to the global environment, which is _G in the interpreter.
 * A refinement might be to isolate the function envioronment from the
 * globals.
 *
 * The implementation rewrites the expr as "return "..expr so that the
 * anonymous function actually produced by lua_load() looks like:
 *
 *     function() return expr end
 *
 *
 * If there is an error and the pmsg parameter is non-NULL, the char *
 * it points to is filled with an error message. The message is
 * allocated by strdup() so the caller is responsible for freeing the
 * storage.
 *
 * Returns a valid cookie or the constant LUA_NOREF (-2).
 */
int le_loadexpr(char *expr, char **pmsg)
{
    int err;
    char *buf;

    if (!L) {
        if (pmsg)
            *pmsg = strdup("LE library not initialized");
        return LUA_NOREF;
    }
    buf = malloc(strlen(expr)+8);
    if (!buf) {
        if (pmsg)
            *pmsg = strdup("Insufficient memory");
        return LUA_NOREF;
    }
    strcpy(buf, "return ");
    strcat(buf, expr);
    err = luaL_loadstring(L,buf);
    free(buf);
    if (err) {
        if (pmsg)
            *pmsg = strdup(lua_tostring(L,-1));
        lua_pop(L,1);
        return LUA_NOREF;
    }
    if (pmsg)
        *pmsg = NULL;
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

/* Evaluate the loaded expression.
 *
 * If there is an error and the pmsg parameter is non-NULL, the char *
 * it points to is filled with an error message. The message is
 * allocated by strdup() so the caller is responsible for freeing the
 * storage.
 *
 * Returns the result or 0 on error.
 */
double le_eval(int cookie, char **pmsg)
{
    int err;
    double ret;

    if (!L) {
        if (pmsg)
            *pmsg = strdup("LE library not initialized");
        return 0;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, cookie);
    err = lua_pcall(L,0,1,0);
    if (err) {
        if (pmsg)
            *pmsg = strdup(lua_tostring(L,-1));
        lua_pop(L,1);
        return 0;
    }
    if (pmsg)
        *pmsg = NULL;
    ret = (double)lua_tonumber(L,-1);
    lua_pop(L,1);
    return ret;
}


/* Free the loaded expression.
 */
void le_unref(int cookie)
{
    if (!L)
        return;
    luaL_unref(L, LUA_REGISTRYINDEX, cookie);
}

/* Set a variable for use in an expression.
 */
void le_setvar(char *name, double value)
{
    if (!L)
        return;
    lua_pushnumber(L,value);
    lua_setglobal(L,name);
}

/* Retrieve the current value of a variable.
 */
double le_getvar(char *name)
{
    double ret;

    if (!L)
        return 0;
    lua_getglobal(L,name);
    ret = (double)lua_tonumber(L,-1);
    lua_pop(L,1);
    return ret;
}
