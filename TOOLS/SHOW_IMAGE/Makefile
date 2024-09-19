TARGETS = show_image
all: $(TARGETS)

XLIBS = -lXm

show_image: show_image.o 
	$(CXXLD) -g show_image.o $(LIB_DIR) $(ALL_LIBS) $(XLIBS) -o show_image
	ln -sf $(PWD)/show_image $(BIN_DIR)/show_image

include ../astro.prog.mk
