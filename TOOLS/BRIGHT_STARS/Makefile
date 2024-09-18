TARGETS = bright_stars
all: $(TARGETS)

bright_stars: bright_stars.o
	$(CXXLD) bright_stars.o -o bright_stars $(LIB_DIR) $(ALL_LIBS) 
	ln -sf $(PWD)/bright_stars $(BIN_DIR)/bright_stars

include ../astro.prog.mk
