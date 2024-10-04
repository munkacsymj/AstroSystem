all: ../ASTRO_LIB/libastro.so libsession_lib.a libsession_lib.so libsession_lib.o \
	libscript_lib.a libscript_lib.so libscript_lib.o test_oa test_plan_exposure \


../ASTRO_LIB/libastro.so : libsession_lib.a libsession_lib.so libsession_lib.o
	(cd ../ASTRO_LIB;make all)

ASTROHOME=$(HOME)

ifndef OPTIMIZE
	OPTIMIZE= -O
endif

CXXFLAGS = $(OPTIMIZE) -Wall -g -DNO_HASH -fPIC -std=c++17 \
	-I../SESSION_LIB \
	-I../REMOTE_LIB \
	-I../IMAGE_LIB  \
	-I../ASTRO_LIB \
	-I $(ASTROHOME)/ASTRO/CFITSIO/cfitsio \
	-I /usr/local/include \
	-I../DATA_LIB   \
	-I /usr/include/eigen3 \



TARGETS = obs_record.o		\
	finder.o                \
	focus_manager.o		\
	gaussian_fit.o          \
	hyperbola.o             \
	mag_from_image.o	\
	plan_exposure.o		\
	running_focus3.o        \
	schedule.o		\
	scheduler.o		\
	scoring.o		\
	session.o		\
	strategy.o		\
	obs_spreadsheet.o	\
	observing_action.o      \
	proc_messages.o         \
	StrategyDatabase.o	\
	validation.o		\

ifeq ($(INTERFACE),INDI)
  ASTROLIBS = ../ASTRO_LIB/libastro.so -lindiclient
else
  ASTROLIBS = ../ASTRO_LIB/libastro.so
endif

SCRIPT_TGTS = script_out.o

focus_simulator: focus_simulator.o
	g++ -o focus_simulator focus_simulator.o -lceres ../ASTRO_LIB/libastro.so -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -L /usr/X11R6/lib -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm -lglog

running_focus2 : running_focus2.o
	g++ -o running_focus2 running_focus2.o -lceres ../ASTRO_LIB/libastro.so -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -L /usr/X11R6/lib -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm -lglog

running_focus3 : running_focus3.o focus_simulator.o
	g++ -o running_focus3 running_focus3.o focus_simulator.o -lceres ../ASTRO_LIB/libastro.so -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -L /usr/X11R6/lib -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm -lglog

test_oa: test_oa.o observing_action.o
	g++ -o test_oa test_oa.o observing_action.o $(ASTROLIBS) -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -lceres -L /usr/X11R6/lib -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm -lglog

test_plan_exposure: test_plan_exposure.o plan_exposure.o
	g++ -o test_plan_exposure test_plan_exposure.o plan_exposure.o $(ASTROLIBS) -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -lceres -L /usr/X11R6/lib -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm -lglog

test_stripper: test_stripper.o
	g++ -o test_stripper test_stripper.o ../ASTRO_LIB/libastro.so -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -lceres -L /usr/X11R6/lib -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm -lglog

script_out.o:		script_out.h


obs_record.o:		obs_record.h
schedule.o:		strategy.h session.h schedule.h
scheduler.o:		scoring.h
mag_from_image.o:	mag_from_image.h
focus_alg.o:		focus_alg.h
scoring.o:		scoring.h
session.o:		session.h strategy.h obs_spreadsheet.h
strategy.o:		strategy.h session.h obs_record.h focus_alg.h mag_from_image.h finder.o
obs_spreadsheet.o:	obs_spreadsheet.h
observing_action.o:     observing_action.h strategy.h finder.o

focus_alg_new_main.o: focus_alg_new.cc
	g++ -c -o focus_alg_new_main.o -DFOCUS_TEST $(CXXFLAGS) focus_alg_new.cc

focus_alg_test: focus_alg_new_main.o
	g++ -g -o focus_alg_test focus_alg_new_main.o ../ASTRO_LIB/libastro.so -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -L /usr/X11R6/lib -lXaw -lXt -lX11 -lpthread -lrt -lcfitsio -lgsl -lgslcblas -lm

libsession_lib.o: $(TARGETS) $(SCRIPT_TGTS)
	ld -g -r -o libsession_lib.o $(TARGETS) $(SCRIPT_TGTS)

libsession_lib.a: libsession_lib.o
	ar -r libsession_lib.a libsession_lib.o
	ranlib libsession_lib.a

libsession_lib.so: libsession_lib.o
	g++ -shared -o libsession_lib.so libsession_lib.o

libscript_lib.a: libsession_lib.o
	ar -r libscript_lib.a libsession_lib.o
	ranlib libscript_lib.a

libscript_lib.so: $(SCRIPT_TGTS)
	g++ -shared -o libscript_lib.so $(SCRIPT_TGTS)

libscript_lib.o: $(SCRIPT_TGTS)
	cp $(SCRIPT_TGTS) libscript_lib.o

clean:
	rm -rf *.o *.a *.so

