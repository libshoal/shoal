# Makefile for streamcluster

SHLPREFIX = .
BASE = ../

TARGET=libshl.so

OBJS += $(SHLPREFIX)/src/misc.o \
	$(SHLPREFIX)/src/linux.o \
	$(SHLPREFIX)/src/shl_array.o \
	$(SHLPREFIX)/src/shl_array_wr-rep.o \
	$(SHLPREFIX)/src/shl_array_conf.o \
	$(SHLPREFIX)/src/shl_timer.o \
	$(SHLPREFIX)/src/shl_multitimer.o \
	$(SHLPREFIX)/src/shl.o

HEADERS=$(wildcard inc/*.hpp) \
	$(wildcard inc/*.h)

# Add external dependencies
# --------------------------------------------------
EXTERNAL_OBJS += $(BASE)contrib/pycrc/crc.o

DEPS = $(OBJS)
DEPS += $(HEADERS)

GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always)

# Switch buildtype: supported are "debug" and "release"
# Do this only if not yet defined (e.g. from parent Makefile)
ifndef BUILDTYPE
	BUILDTYPE := release
endif

CXXFLAGS += -Wall

ifdef SHL_PAPI
	CXXFLAGS += -DPAPI
endif

ifeq ($(BUILDTYPE),debug)
	CXXFLAGS +=-ggdb -O0 -pg -DSHL_DEBUG
else
	CXXFLAGS += -O3
endif

INC += -I$(BASE)contrib/numactl-2.0.9 \
	-I$(SHLPREFIX)/inc \
	-I$(SHLPREFIX)/inc/backend/linux \
	-I$(BASE)contrib/papi-5.3.0/src \
	-I$(BASE)contrib/pycrc/

LIBS += -L$(BASE)contrib/numactl-2.0.9 -lnuma \
	-L$(BASE)contrib/papi-5.3.0/src -lpapi \
	-L$(BASE)contrib/papi-5.3.0/src/libpfm4/lib -lpfm

CXXFLAGS += -DVERSION=\"$(GIT_VERSION)\" -fPIC -fpic -pthread -fopenmp -std=c++11 -Wall

all: shoal

# LUA
# --------------------------------------------------
LUA_BASE := $(BASE)/contrib/lua-5.2.3/
LUA := $(LUA_BASE)src/
INC += -I$(LUA)
LIBS += -lm -ldl
OBJS += $(LUA)/liblua.a

EXTERNAL_OBJS += $(LUA)/liblua.a

$(LUA)/liblua.a:
	make -C $(LUA_BASE) linux

# PAPI
# --------------------------------------------------
PAPI_BASE := $(BASE)/contrib/papi-5.3.0/
PAPI := $(PAPI_BASE)/src/

$(PAPI)/libpapi.so:
	cd $(PAPI); ./configure
	make -C $(PAPI)

EXTERNAL_OBJS += $(PAPI)/libpapi.so

# NUMA
# --------------------------------------------------

NUMA_BASE := $(BASE)/contrib/numactl-2.0.9/

$(NUMA_BASE)/libnuma.so:
	make -C $(NUMA_BASE)

EXTERNAL_OBJS += $(NUMA_BASE)/libnuma.so

# Build shared library
# --------------------------------------------------
shoal: $(DEPS) $(EXTERNAL_OBJS)
	$(CXX) -shared $(OBJS) $(EXTERNAL_OBJS) $(LIBS) -o $(TARGET)

# Compile object files
# --------------------------------------------------
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

%.o : %.c
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

debug:
	echo $(HEADERS)
