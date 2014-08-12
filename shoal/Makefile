# Makefile for streamcluster

# Configuration
# --------------------------------------------------
SHLPREFIX = .
BASE = ../
BFSRC = $(HOME)/bf/git/
BFBLD = $(HOME)/bf/git/build
# --------------------------------------------------

TARGET=libshl.so

CXXFLAGS := $(CXXFLAGS)
OBJS := $(SHLPREFIX)/src/misc.o \
	$(SHLPREFIX)/src/shl.o

DEPS = $(OBJS)
DEPS += $(wildcard $(BASE)/inc/*)

# Choose operating system to build for
TARGET := Barrelfish

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

LIBS :=
INC := 	-I$(BASE)shoal/inc
CXXFLAGS += -DVERSION=\"$(GIT_VERSION)\" -fPIC -fopenmp -std=c++11 -Wall


ifeq ($(TARGET),Barrelfish)
	OBJS += $(SHLPREFIX)/src/barrelfish.o
	CXXFLAGS += -DBARRELFISH -nostdinc -nostdinc++
	INC += -I$(BFSRC)/include/cxx/ \
		-I$(BFSRC)/include  \
		-I$(BFSRC)/include/arch/x86_64  \
		-I$(BFSRC)/lib/newlib/newlib/libc/include  \
		-I$(BFSRC)/include/c  \
		-I$(BFSRC)/include/target/x86_64  \
		-I$(BFSRC)/include/ipv4  \
		-I$(BFSRC)/usr/tests/cxx  \
		-I$(BFBLD)/x86_64/usr/tests/cxx \
		-I$(BFBLD)/x86_64/include  \
		-I$(BFBLD)/x86_64/include/dev

else
	INC += -I$(BASE)contrib/numactl-2.0.9 \
		-I$(BASE)contrib/papi-5.3.0/src
	LIBS += -L$(BASE)contrib/numactl-2.0.9 -lnuma \
		-L$(BASE)contrib/papi-5.3.0/src -lpapi \
		-L$(BASE)contrib/papi-5.3.0/src/libpfm4/lib -lpfm
	OBJS += $(SHLPREFIX)/src/linux.o
	CXXFLAGS += -DLINUX
endif

all: $(DEPS)
	$(CXX) $(CXXFLAGS) $(INC) -shared $(OBJS) $(LIBS) -o $(TARGET)

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

%.o : %.c
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)
