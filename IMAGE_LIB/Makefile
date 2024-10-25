all: libImage.a libImage.so

ASTROHOME=$(HOME)

ifeq ($(INTERFACE),INDI)
  INTERFACE_DEF= -D USEINDI -D INTERFACE=INDI
  INTERFACE_FILES = astro_indi.o blocker_indi.o cfw_indi.o cooler_indi.o focuser_indi.o mount_indi.o \
	scope_api_indi.o camera_indi.o camera_api_indi.o
else ifeq ($(INTERFACE),NATIVE)
  INTERFACE_DEF= -D INTERFACE=NATIVE
  INTERFACE_FILES = scope_api.o camera_api.o
else
  $(error Environment variable INTERFACE not defined as INDI or NATIVE)
endif

INCLUDES = -I../REMOTE_LIB \
	-I../IMAGE_LIB \
	-I../DATA_LIB \
	-I../ASTRO_LIB \
	-I$(ASTROHOME)/ASTRO/CFITSIO/cfitsio

#OPT_FLAGS = -O0
OPT_FLAGS = -O $(INTERFACE_DEF) 

CXXFLAGS = $(OPT_FLAGS) -Wall -g -DLINUX -fno-strict-aliasing $(INCLUDES) \
	-D_FILE_OFFSET_BITS=64 -fPIC

TARGETS = background.o \
	bad_pixels.o \
	Coordinates.o \
	dark.o \
	Filter.o \
	Image.o \
	IStarList.o \
	nlls_general.o \
	screen_image.o \
	Statistics.o \
	Tracker.o \
	wcs.o 

libImage.o : $(TARGETS)
	ld -r -o libImage.o $(TARGETS)

libImage.a: libImage.o
	ar -r libImage.a libImage.o
	ranlib libImage.a

libImage.so: libImage.o
	g++ -shared -o libImage.so libImage.o


clean: 
	rm -rf *.o libImage.a libImage.so
