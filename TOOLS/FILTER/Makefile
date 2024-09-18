TARGETS = filter
all: $(TARGETS)

filter: filter.o
	$(CXXLD) filter.o -o filter $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/filter $(BIN_DIR)/filter

filter.o: filter.cc

include ../astro.prog.mk
