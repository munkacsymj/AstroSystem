TARGETS = stack

all: $(TARGETS)

stack: stack.o simple_stack.o
	$(CXXLD) stack.o simple_stack.o -o stack $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/stack $(BIN_DIR)/stack

stack.o: stack.cc

include ../astro.prog.mk
