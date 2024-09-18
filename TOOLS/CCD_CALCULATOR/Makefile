TARGETS = measure_stars measure_glow

all: $(TARGETS)

measure_stars: measure_stars.o
	$(CXXLD) measure_stars.o -o measure_stars $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/measure_stars $(BIN_DIR)/measure_stars

measure_stars.o: measure_stars.cc

measure_glow: measure_glow.o
	$(CXXLD) measure_glow.o -o measure_glow $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/measure_glow $(BIN_DIR)/measure_glow

measure_banding: measure_banding.o
	$(CXXLD) measure_banding.o -o measure_banding $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/measure_banding $(BIN_DIR)/measure_banding

fixup: fixup.o
	$(CXXLD) fixup.o -o fixup $(LIB_DIR) $(ALL_LIBS)

measure_glow.o: measure_glow.cc

include ../astro.prog.mk
