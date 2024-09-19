TARGETS = synthetic_flat gradient fix_gradient

all: $(TARGETS)

synthetic_flat: synthetic_flat.o
	$(CXXLD) synthetic_flat.o -o synthetic_flat $(LIB_DIR) $(ALL_LIBS)

gradient: gradient.o
	$(CXXLD) gradient.o -o gradient $(LIB_DIR) $(ALL_LIBS)

fix_gradient: fix_gradient.o
	$(CXXLD) fix_gradient.o -o fix_gradient $(LIB_DIR) $(ALL_LIBS)

include ../astro.prog.mk
