# Add external dependencies
# --------------------------------------------------
EXTERNAL_OBJS += $(BASE)contrib/pycrc/crc.o

# LUA
# --------------------------------------------------
INC += -I/usr/include/lua5.2/
LIBS += -lm -ldl -L \
	-L/usr/lib/x86_64-linux-gnu/ -llua5.2 \

