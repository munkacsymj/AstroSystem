TARGETS = execute_script
OBJECTS = eval.o execute_script.o lex.o
ALL_LIBS = -lastro -lcfitsio -lgsl -lgslcblas -lXt -lX11 -lXaw -lm
all: $(TARGETS)

execute_script: $(OBJECTS)
	$(CXXLD) -o execute_script -g $(LIB_DIR) $(OBJECTS) $(ALL_LIBS)
	ln -sf $(PWD)/execute_script $(BIN_DIR)/execute_script

include ../astro.prog.mk
