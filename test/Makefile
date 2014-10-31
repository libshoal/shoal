LUA := ../contrib/lua-5.2.3/src/
CFLAGS += -I$(LUA)

t-le: t-le.c le.c $(LUA)/liblua.a -lm -ldl
	$(CC) $(CFLAGS) -o $@ $^
