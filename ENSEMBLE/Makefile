OBJECTS= build_ensemble.o

TARGETS= build_ensemble
all: $(TARGETS)

build_ensemble: $(OBJECTS)
	$(CXXLD) -g $(OBJECTS) $(LIB_DIR) $(ALL_LIBS) -o build_ensemble

include ../astro.prog.mk
