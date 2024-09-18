CXXLD	= g++
CCLD	= gcc

ASTRO_HOME := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))/..
ASTROHOME=$(HOME)

ifndef OPTIMIZE
OPTIMIZE = -O3
endif

CXXFLAGS	= -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
	$(INCLUDES) -g $(OPTIMIZE) -Wall -fPIC -std=gnu++17

INCLUDES = $(EXTRA_INCLUDES) \
	-I $(ASTROHOME)/ASTRO/CFITSIO/cfitsio \
	-I $(ASTRO_HOME)/IMAGE_LIB \
	-I $(ASTRO_HOME)/REMOTE_LIB \
	-I $(ASTRO_HOME)/ASTRO_LIB \
	-I $(ASTRO_HOME)/DATA_LIB \
	-I $(ASTRO_HOME)/SESSION_LIB \



LIB_DIR = \
	-L $(ASTRO_HOME)/ASTRO_LIB \
	-L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio \
	-L /usr/X11R6/lib \
	-Wl,-rpath,$(LIBS_HOME)/ASTRO_LIB


LIBS_HOME = $(ASTRO_HOME)

BIN_DIR = $(ASTRO_HOME)/BIN/

# The order of these libraries does matter.
ALL_LIBS = $(LIBS_HOME)/ASTRO_LIB/libastro.so \
	-lceres \
	-lcfitsio \
	-L/usr/X11R6/lib -lXaw -lXt -lX11 \
	-lpthread -lrt -lglog \
	-lgsl -lgslcblas -lm

#ALL_LIBS = -lsession_lib -lImage -ldata_lib -lremote_lib -lImage -lcfitsio -lgsl -lgslcblas -lm

clean:
	rm -f *.o $(TARGETS)

all:	$(TARGETS)
