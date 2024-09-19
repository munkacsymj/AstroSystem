TARGETS = list_stars convert_coords obs_matrix
all: $(TARGETS)

list_stars: list_stars.o
	$(CXXLD) list_stars.o -o list_stars $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/list_stars $(BIN_DIR)/list_stars

list_stars.o: list_stars.cc

obs_matrix: obs_matrix.o
	$(CXXLD) obs_matrix.o -o obs_matrix $(LIB_DIR) $(ALL_LIBS)

obs_matrix.o: obs_matrix.cc

convert_coords: convert_coords.o
	$(CXXLD) convert_coords.o -o convert_coords $(LIB_DIR) $(ALL_LIBS)

convert_coords.o: convert_coords.cc

include ../astro.prog.mk
