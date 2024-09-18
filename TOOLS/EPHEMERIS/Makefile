TARGETS = ephemeris
all: $(TARGETS)

ephemeris: ephemeris.o 
	$(CXXLD) ephemeris.o -o ephemeris $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/ephemeris $(BIN_DIR)/ephemeris

include ../astro.prog.mk
