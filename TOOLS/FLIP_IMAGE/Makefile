TARGETS = flip_image
all: $(TARGETS)

flip_image: flip_image.o 
	$(CXXLD) flip_image.o -o flip_image $(LIB_DIR) $(ALL_LIBS)

flip_image.o: flip_image.cc

include ../astro.prog.mk
