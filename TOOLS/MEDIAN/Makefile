TARGETS = median calibrate convert_fits
#ALL_LIBS = -lastro -lcfitsio -lgsl -lgslcblas -lXt -lX11 -lXaw -lm

all: $(TARGETS)

median: median.o 
	$(CXXLD) -g median.o $(LIB_DIR) $(ALL_LIBS) -o median
	ln -sf $(PWD)/median $(BIN_DIR)/median
	ln -sf $(PWD)/median $(BIN_DIR)/average
	ln -sf $(PWD)/median $(BIN_DIR)/medianaverage
	ln -sf $(PWD)/median $(BIN_DIR)/subtract
	ln -sf $(PWD)/median $(BIN_DIR)/add

calibrate: calibrate.o
	$(CXXLD) -g calibrate.o $(LIB_DIR) $(ALL_LIBS) -o calibrate
	ln -sf $(PWD)/calibrate $(BIN_DIR)/calibrate

print_pixel_values: print_pixel_values.o
	$(CXXLD) -g print_pixel_values.o $(LIB_DIR) $(ALL_LIBS) -o print_pixel_values

convert_fits: convert_fits.o
	$(CXXLD) -g convert_fits.o $(LIB_DIR) $(ALL_LIBS) -o convert_fits
	ln -sf $(PWD)/convert_fits $(BIN_DIR)/convert_fits

include ../astro.prog.mk
