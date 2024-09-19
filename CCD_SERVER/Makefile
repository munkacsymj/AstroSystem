all: ccd_server set_cooler_off test_cooler

#CXXFLAGS=-g -I../sbig-udrv -I../CFITSIO/cfitsio -I../REMOTE_LIB -I../IMAGE_LIB
CXXFLAGS=-g -I/home/mark/ASTRO/CFITSIO/cfitsio -I../REMOTE_LIB -I../IMAGE_LIB -I../DATA_LIB -I../ASTRO_LIB

#LDFLAGS= ../sbig-udrv/udrv4.0.a ../REMOTE_LIB/libremote_lib.a /usr/local/src/cfitsio/libcfitsio.a -lm
LDFLAGS= ../REMOTE_LIB/libremote_lib.a ../IMAGE_LIB/libImage.a ../DATA_LIB/libdata_lib.a /home/mark/ASTRO/CFITSIO/cfitsio/libcfitsio.a -lqhyccd -lcurl -L/usr/X11R6/lib -lXaw -lXt -lX11 -lgsl -lgslcblas -lm -lpthread

OBJECTS = ccd_server.o \
	ccd_message_handler.o \
	usb_cfw.o \
	ambient.o \
	cooler.o \



clean:
	rm -rf $(OBJECTS) ccd_server

ccd_server: $(OBJECTS)
	g++ $(OBJECTS) $(LDFLAGS) -o ccd_server

test_cooler: test_cooler.o
	g++ test_cooler.o $(LDFLAGS) -o test_cooler

set_cooler_off: set_cooler_off.o
	g++ set_cooler_off.o $(LDFLAGS) -o set_cooler_off

ccd_message_handler.o: ccd_message_handler.h ccd_server.h usb_cfw.h
usb_cfw.o: usb_cfw.h

ccd_server.o: ccd_server.h ccd_message_handler.h

