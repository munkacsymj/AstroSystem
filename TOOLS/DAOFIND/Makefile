include ../astro.prog.mk

OBJECTS= apbfdfind.o apconvolve.o egauss.o find_stars.o fwhm.o

TARGETS = find_stars test_stars

#CXXFLAGS += -pg 

all: $(TARGETS)

find_stars: $(OBJECTS)
	$(CXXLD) $(OBJECTS) -o find_stars $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/find_stars $(BIN_DIR)/find_stars

test_stars: test_stars.o
	$(CXXLD) test_stars.o -o test_stars $(LIB_DIR) $(ALL_LIBS)



