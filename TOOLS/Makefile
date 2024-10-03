TARGETS = make_flat auto_flat photometry setup_iraf \
	auto_all_filter_flat auto_no_filter_flat flatten allstar2istar photometry_psf \
	update_pst_list image_statistics flat_renormalize fits_16 \
	assess_flat print256_256

all: $(TARGETS)

#	alt_photometry

allstar2istar: allstar2istar.o
	$(CXXLD) allstar2istar.o -o allstar2istar $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/allstar2istar $(BIN_DIR)/allstar2istar

print256_256: print256_256.o
	$(CXXLD) print256_256.o -o print256_256 $(LIB_DIR) $(ALL_LIBS)

flat_renormalize: flat_renormalize.o
	$(CXXLD) flat_renormalize.o -o flat_renormalize $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/flat_renormalize $(BIN_DIR)/flat_renormalize

assess_flat: assess_flat.o
	$(CXXLD) assess_flat.o -o assess_flat $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/assess_flat $(BIN_DIR)/assess_flat

fits_16: fits_16.o
	$(CXXLD) fits_16.o -o fits_16 $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/fits_16 $(BIN_DIR)/fits_16

update_pst_list: update_pst_list.o
	$(CXXLD) update_pst_list.o -o update_pst_list $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/update_pst_list $(BIN_DIR)/update_pst_list

photometry_psf: photometry_psf.o
	$(CXXLD) photometry_psf.o -o photometry_psf $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/photometry_psf $(BIN_DIR)/photometry_psf

auto_all_filter_flat: auto_all_filter_flat.o
	$(CXXLD) auto_all_filter_flat.o -o auto_all_filter_flat $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/auto_all_filter_flat $(BIN_DIR)/auto_all_filter_flat

auto_no_filter_flat: auto_no_filter_flat.o
	$(CXXLD) auto_no_filter_flat.o -o auto_no_filter_flat $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/auto_no_filter_flat $(BIN_DIR)/auto_no_filter_flat

flatten: flatten.o
	$(CXXLD) flatten.o -o flatten $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/flatten $(BIN_DIR)/flatten

make_flat: make_flat.o
	$(CXXLD) make_flat.o -o make_flat $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/make_flat $(BIN_DIR)/make_flat

make_flat.o: make_flat.cc

image_statistics: image_statistics.o
	$(CXXLD) image_statistics.o -o image_statistics $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/image_statistics $(BIN_DIR)/image_statistics

test_image: test_image.o
	$(CXXLD) test_image.o -o test_image $(LIB_DIR) $(ALL_LIBS)

image_statistics.o: image_statistics.cc

auto_flat: auto_flat.o
	$(CXXLD) auto_flat.o -o auto_flat $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/auto_flat $(BIN_DIR)/auto_flat

auto_flat.o: auto_flat.cc

photometry: photometry.o
	$(CXXLD) photometry.o -o photometry $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/photometry $(BIN_DIR)/photometry

photometry.o: photometry.cc

setup_iraf: setup_iraf.o
	$(CXXLD) setup_iraf.o -o setup_iraf $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/setup_iraf $(BIN_DIR)/setup_iraf

setup_iraf.o: setup_iraf.cc

alt_photometry: alt_photometry.o
	$(CXXLD) alt_photometry.o -o alt_photometry $(LIB_DIR) $(ALL_LIBS)

alt_photometry.o: alt_photometry.cc

alt1_photometry: alt1_photometry.o
	$(CXXLD) alt1_photometry.o -o alt1_photometry $(LIB_DIR) $(ALL_LIBS)

alt1_photometry.o: alt1_photometry.cc

test_coordinates: test_coordinates.o
	$(CXXLD) test_coordinates.o -o test_coordinates $(LIB_DIR) $(ALL_LIBS)

test_coordinates.o: test_coordinates.cc

include ../astro.prog.mk
