# Makefile for streamcluster

SHLPREFIX = .
BASE = ../

TARGET=libshl.so

CXXFLAGS := $(CXXFLAGS)
OBJS += $(SHLPREFIX)/src/misc.o \
	$(SHLPREFIX)/src/linux.o \
	$(SHLPREFIX)/src/shl.o

DEPS = $(OBJS)
DEPS += $(wildcard $(BASE)/inc/*)

GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always)

# Switch buildtype: supported are "debug" and "release"
# Do this only if not yet defined (e.g. from parent Makefile)
ifndef BUILDTYPE
	BUILDTYPE := release
endif

ifeq ($(BUILDTYPE),debug)
	CXXFLAGS +=-g -O0 -pg -DSHL_DEBUG
else
	CXXFLAGS += -O3
endif

INC += -I$(BASE)contrib/numactl-2.0.9 \
	-I$(BASE)shoal/inc \
	-I$(BASE)contrib/papi-5.3.0/src
LIBS += -L$(BASE)contrib/numactl-2.0.9 -lnuma \
	-L$(BASE)contrib/papi-5.3.0/src -lpapi \
	-L$(BASE)contrib/papi-5.3.0/src/libpfm4/lib -lpfm
CXXFLAGS += -DVERSION=\"$(GIT_VERSION)\" -fPIC -fopenmp -std=c++11 -Wall

all: $(DEPS)
	$(CXX) $(CXXFLAGS) $(INC) -shared $(OBJS) $(LIBS) -o $(TARGET)

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

%.o : %.c
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)
