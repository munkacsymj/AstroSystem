TARGETS = show_sequence 

all: $(TARGETS)

XLIBS = -lXm

show_sequence: show_sequence.o mini_win.o
	$(CXXLD) -g show_sequence.o mini_win.o $(LIB_DIR) $(ALL_LIBS) $(XLIBS) -o show_sequence

include ../astro.prog.mk
