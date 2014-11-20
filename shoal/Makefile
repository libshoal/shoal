# Makefile for streamcluster

SHLPREFIX = .
BASE = ../

TARGET=libshl.so

OBJS += $(SHLPREFIX)/src/misc.o \
	$(SHLPREFIX)/src/linux.o \
	$(SHLPREFIX)/src/shl_array.o \
	$(SHLPREFIX)/src/shl_array_wr-rep.o \
	$(SHLPREFIX)/src/shl.o

EXTERNAL_OBJS += $(BASE)contrib/pycrc/crc.o

DEPS = $(OBJS)
DEPS += $(wildcard $(BASE)/inc/*)

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
	CXXFLAGS +=-g -O0 -pg -DSHL_DEBUG
else
	CXXFLAGS += -O3
endif

INC += -I$(BASE)contrib/numactl-2.0.9 \
	-I$(BASE)shoal/inc \
	-I$(BASE)contrib/papi-5.3.0/src \
	-I$(BASE)contrib/pycrc/

LIBS += -L$(BASE)contrib/numactl-2.0.9 -lnuma \
	-L$(BASE)contrib/papi-5.3.0/src -lpapi \
	-L$(BASE)contrib/papi-5.3.0/src/libpfm4/lib -lpfm

CXXFLAGS += -DVERSION=\"$(GIT_VERSION)\" -fPIC -pthread -fopenmp -std=c++11 -Wall

# Build shared library
# --------------------------------------------------
all: $(DEPS) $(EXTERNAL_OBJS)
	$(CXX) $(CXXFLAGS) $(INC) -shared $(OBJS) $(EXTERNAL_OBJS) $(LIBS) -o $(TARGET)

# Compile object files
# --------------------------------------------------
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

%.o : %.c
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)
