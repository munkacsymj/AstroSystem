TARGETS = show_catalog
all: $(TARGETS)

XLIBS = -lXm

show_catalog: show_catalog.o 
	$(CXXLD) -g show_catalog.o $(LIB_DIR) $(ALL_LIBS) $(XLIBS) -o show_catalog
	ln -sf $(PWD)/show_catalog $(BIN_DIR)/show_catalog

include ../astro.prog.mk
