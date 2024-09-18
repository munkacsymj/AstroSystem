TARGETS = cooler
all: $(TARGETS)

cooler: cooler.o 
	$(CXXLD) cooler.o -o cooler $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/$(@F) $(BIN_DIR)/$(@F)

cooler.o: cooler.cc

include ../astro.prog.mk
