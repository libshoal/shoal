#include <cstdio>
#include <cstring>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
int main (void) {
    char buff[256];

    sprintf(buff, "print(\"Hello world\")\n");

    int error;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L); /* opens Lua */

    while (fgets(buff, sizeof(buff), stdin) != NULL) {
        error = luaL_loadbuffer(L, buff, strlen(buff), "line") ||
            lua_pcall(L, 0, 0, 0);
        if (error) {
            fprintf(stderr, "%s", lua_tostring(L, -1));
            lua_pop(L, 1);  /* pop error message from the stack */
        }
    }

    lua_close(L);
    return 0;
}
