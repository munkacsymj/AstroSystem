
OBJECTS= telescope.o

TARGETS = telescope 

all: $(TARGETS) 

telescope: $(OBJECTS)
	$(CXXLD) $(OBJECTS) -o telescope $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/telescope $(BIN_DIR)/telescope


include ../astro.prog.mk
