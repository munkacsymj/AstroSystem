PARAB    = hyperbola
PARAB_H  = $(PARAB).h
PARAB_CC = $(PARAB).cc
PARAB_O  = $(PARAB).o

TARGETS = focus test_focus  # parab_test test_composite fine_focus test_hyperbola

OBJECTS = focus1.o \
	focus_star.o \
	focus_main.o

all: focus 

fine_focus.o:	fine_focus.cc
focus.o: 	focus.cc nlls_simple.h $(PARAB_H)
nlls_simple.o: 	nlls_simple.cc nlls_simple.h
$(PARAB_O): 	$(PARAB_H) $(PARAB_CC)
focus_main.o:	focus_main.cc focus.h
parab_test.o:   $(PARAB_H)

fine_focus: fine_focus.o
	$(CXXLD) -g fine_focus.o $(LIB_DIR) $(ALL_LIBS) -o fine_focus

refocus: refocus.o 
	$(CXXLD) -g refocus.o $(LIB_DIR) $(ALL_LIBS) -o refocus

focus: $(OBJECTS)
	$(CXXLD) -pthread -g $(OBJECTS) $(LIB_DIR) $(ALL_LIBS) -o focus
	ln -sf $(PWD)/focus $(BIN_DIR)/focus

parab_test: parab_test.o $(PARAB_O)
	$(CXXLD) -g parab_test.o $(PARAB_O) $(LIB_DIR) $(ALL_LIBS) -o parab_test

test_hyperbola: test_hyperbola.o hyperbola.o
	$(CXXLD) -g test_hyperbola.o $(PARAB_O) $(LIB_DIR) $(ALL_LIBS) -o test_hyperbola

test_focus: test_focus.o
	$(CXXLD) -g test_focus.o $(LIB_DIR) $(ALL_LIBS) -o test_focus

test_composite: test_composite.o
	$(CXXLD) -g test_composite.o $(LIB_DIR) $(ALL_LIBS) -o test_composite

include ../astro.prog.mk

