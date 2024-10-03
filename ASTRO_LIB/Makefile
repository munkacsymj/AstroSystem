all: libastro.so
clean:
	rm -f *.o *.so

COMPONENTS = ../DATA_LIB/libdata_lib.o \
	../IMAGE_LIB/libImage.o \
	../SESSION_LIB/libsession_lib.o \
	../REMOTE_LIB/libremote_lib.o

libastro.so : $(COMPONENTS)
	ld -g -r -o libastro.o $(COMPONENTS)
	g++ -g -shared -o libastro.so libastro.o

