TARGETS = graph_star

all: $(TARGETS)

XLIBS = -lXm

graph_star: graph_star.o 
	$(CXXLD) -g graph_star.o $(LIB_DIR) $(ALL_LIBS) $(XLIBS) -o graph_star

include ../astro.prog.mk
