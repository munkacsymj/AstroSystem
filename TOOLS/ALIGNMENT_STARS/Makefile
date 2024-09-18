TARGETS = alignment_stars
all: $(TARGETS)

alignment_stars: alignment_stars.o 
	$(CXXLD) -g alignment_stars.o $(LIB_DIR) $(ALL_LIBS)  -o alignment_stars

include ../astro.prog.mk
