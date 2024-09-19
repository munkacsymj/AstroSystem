TARGETS = move
all: $(TARGETS)

move: move.o
	$(CXXLD) move.o -o move $(LIB_DIR) $(ALL_LIBS)
	ln -f -s $(PWD)/move $(BIN_DIR)/move

move.o: move.cc

include ../astro.prog.mk
