TARGETS = finder
all: $(TARGETS)

finder: finder.o 
	$(CXXLD) -g finder.o $(LIB_DIR) $(ALL_LIBS)  -o finder
	ln -f -s $(PWD)/finder $(BIN_DIR)/finder

include ../astro.prog.mk
