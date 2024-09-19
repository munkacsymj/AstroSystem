OBJECTS = idle_expose.o
TARGETS = idle_expose  
all: $(TARGETS)

idle_expose: idle_expose.o
	$(CXXLD) -g idle_expose.o $(LIB_DIR) $(ALL_LIBS) -o idle_expose
	ln -s -f $(PWD)/idle_expose $(BIN_DIR)/idle_expose

include ../astro.prog.mk
