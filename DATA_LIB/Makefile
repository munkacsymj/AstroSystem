all: ../ASTRO_LIB/libastro.so libdata_lib.a libdata_lib.so #json_test

../ASTRO_LIB/libastro.so : libdata_lib.a libdata_lib.so
	(cd ../ASTRO_LIB;make all)

clean:
	rm -rf *.a *.so *.o

ifneq ($(HOSTNAME),jellybean)
HOSTDEF= -D POGO
else
HOSTDEF= -D JELLYBEAN
endif
ifeq ($(HOSTNAME),NewWorkshop)
HOSTDEF= -D POGO -D LINUX
endif

ifndef OPTIMIZE
	OPTIMIZE = -O
endif

CXXFLAGS = $(OPTIMIZE) -Wall -g -I../IMAGE_LIB -I../ASTRO_LIB -I../REMOTE_LIB -I../CFITSIO/cfitsio $(HOSTDEF) -fPIC

TARGETS = named_stars.o \
	dbase.o \
	bvri_db.o \
	HGSC.o \
	json.o \
	astro_db.o \
	TCS.o \
	report_file.o \
	bright_star.o \
	system_config.o \
	work_queue.o


named_stars.o:		named_stars.h
dbase.o:                dbase.h
HGSC.o:			HGSC.h
TCS.o:			TCS.h
bright_star.o:		bright_star.h
report_file.o:          report_file.h
bvri_db.o:		bvri_db.h dbase.h
work_queue.o:           work_queue.cc work_queue.h
json.o:                 json.h
astro_db.o:             json.h astro_db.h

libdata_lib.o: $(TARGETS)
	ld -r -o libdata_lib.o $(TARGETS)

libdata_lib.a: libdata_lib.o
	ar -r libdata_lib.a libdata_lib.o
	ranlib libdata_lib.a

libdata_lib.so: $(TARGETS)
	g++ -shared -o libdata_lib.so libdata_lib.o

test_wq: test_wq.o work_queue.o
	g++ -o test_wq test_wq.o work_queue.o -L ~/ASTRO/CFITSIO/cfitsio -L /usr/X11R6/lib ../ASTRO_LIB/libastro.so -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm

json_test: json_test.o astro_db.o
	g++ -o json_test json_test.o astro_db.o -L ~/ASTRO/CFITSIO/cfitsio -L /usr/X11R6/lib ../ASTRO_LIB/libastro.so -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm

