all: libImage.a libImage.so

ASTROHOME=$(HOME)

INCLUDES = -I../REMOTE_LIB \
	-I../IMAGE_LIB \
	-I../DATA_LIB \
	-I../ASTRO_LIB \
	-I$(ASTROHOME)/ASTRO/CFITSIO/cfitsio

#OPT_FLAGS = -O0
OPT_FLAGS = -O

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
