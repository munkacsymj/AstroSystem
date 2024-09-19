TARGETS = time_seq  analyze_time_seq test_focus2 # genetic_test test_focus 

all: $(TARGETS)

time_seq: time_seq.o finder.o
	$(CXXLD) time_seq.o finder.o -o time_seq $(LIB_DIR) $(ALL_LIBS)

test_focus2: test_focus2.o
	$(CXXLD) test_focus2.o -o test_focus2 $(LIB_DIR) $(ALL_LIBS)

analyze_time_seq: analyze_time_seq.o
	$(CXXLD) analyze_time_seq.o -o analyze_time_seq $(LIB_DIR) $(ALL_LIBS)

focus_seq: focus_seq.o
	$(CXXLD) focus_seq.o -o focus_seq $(LIB_DIR) $(ALL_LIBS)

test_focus: test_focus.o running_focus.o
	$(CXXLD) test_focus.o running_focus.o -o test_focus $(LIB_DIR) $(ALL_LIBS)

test_genetic: genetic_focus.o running_focus.o test_genetic.o
	$(CXXLD) $^ -o $@ $(LIB_DIR) $(ALL_LIBS)

time_seq.o: time_seq.cc
test_focus.o running_focus.o: running_focus.h running_focus_int.h running_focus.cc

include ../astro.prog.mk
