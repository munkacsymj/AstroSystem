TARGETS = set_focus
all: $(TARGETS)

OBJECTS = set_focus.o

set_focus: $(OBJECTS)
	$(CXXLD) -g $(OBJECTS) $(LIB_DIR) $(ALL_LIBS) -o set_focus
	ln -f -s $(PWD)/set_focus $(BIN_DIR)/set_focus


set_focus.o: 	set_focus.cc

include ../astro.prog.mk

