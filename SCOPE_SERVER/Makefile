all: focus_server focuser_reset focuser_test c14focuser esatto

FOCUS_OBJ = c14focuser.o esatto_focuser.o

LOCAL_OBJS = $(FOCUS_OBJ) \
	prb.o \
	json.o \
	track.o \
	json.o \
	prb.o \
	arduino_serial_lib.o \
	flatlight.o \
	focus_server.o \
	lx200.o \
	scope_message_handler.o

LIBS = \
       ../REMOTE_LIB/libremote_lib.a \
       ../IMAGE_LIB/libImage.a \
       ../DATA_LIB/libdata_lib.a \
       ../../CFITSIO/cfitsio/libcfitsio.a \
       -L/usr/X11R6/lib -lXaw -lXt -lX11 -lgsl -lgslcblas \
       -lm  -lpthread

CXXFLAGS=-g -I../REMOTE_LIB -I../ASTRO_LIB
CFLAGS=-g -I../REMOTE_LIB -I../ASTRO_LIB

clean:
	rm -f *.o focuser_reset focus_server

c14focuser: c14focuser.o arduino_serial_lib.o
	g++ -o c14focuser c14focuser.o arduino_serial_lib.o -lpthread

c14focuser.o: c14focuser.cc
	g++ -c c14focuser.cc

esatto: esatto.o arduino_serial_lib.o json.o
	g++ -o esatto esatto.o arduino_serial_lib.o json.o -lpthread

esatto.o: esatto.cc json.h
	g++ -c esatto.cc

focuser_reset: focuser_reset.c
	g++ focuser_reset.c -o focuser_reset

focuser_test: focuser_test.c
	g++ focuser_test.c -o focuser_test

focus_server: $(LOCAL_OBJS)
	g++ $(LOCAL_OBJS) -g -o focus_server $(LIBS)

focus_server.o: lx200.h focus.h scope_message_handler.h

track.o: track.h

focus.o: focus.h

focus_jmi.o: focus.h

lx200.o: lx200.h

scope_message_handler.o: scope_message_handler.h focus.h track.h
arduino_serial_lib.o: arduino_serial_lib.c arduino_serial_lib.h

