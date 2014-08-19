all: test

LUA := ../../contrib/lua-5.2.3/src
FLAGS := -Wall -I$(LUA)
LIBS := -static -L$(LUA) -llua -ldl
CPP := g++

test: test.cc
	$(CPP) $(FLAGS) $^ $(LIBS) -o $@
