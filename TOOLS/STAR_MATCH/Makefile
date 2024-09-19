#OPTIMIZE= -O0
OPTIMIZE= -O3 # -pg

OBJECTS= star_match.o correlate1.o aperture_phot.o high_precision.o
OBJECTS2= star_match2.o correlate2.o aperture_phot.o high_precision.o matcher.o
OBJECTS3= star_match3.o correlate3.o aperture_phot.o matcher3.o

OBJECT_CHECK= check_correlate.o correlate1.o aperture_phot.o 

TARGETS = star_match3 delete_stars add_star analyze # prep_for_pinpoint test_image test_center find_stars star_match

all: $(TARGETS) $(BIN_DIR)/full_script

$(BIN_DIR)/full_script:
	ln -sf $(PWD)/full_script $(BIN_DIR)/full_script

star_match: $(OBJECTS)
	$(CXXLD) $(OBJECTS) -o star_match $(LIB_DIR) $(ALL_LIBS)
	#ln -sf $(PWD)/star_match $(BIN_DIR)/star_match

star_match3: $(OBJECTS3)
	$(CXXLD) $(OBJECTS3) -o star_match3 $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/star_match3 $(BIN_DIR)/star_match

test_image: test_image.o
	$(CXXLD) test_image.o -o test_image $(LIB_DIR) $(ALL_LIBS)

test_center: test_center.o
	$(CXXLD) test_center.o -o test_center $(LIB_DIR) $(ALL_LIBS)

find_stars: find_stars.o
	$(CXXLD) find_stars.o -o find_stars $(LIB_DIR) $(ALL_LIBS)

delete_stars: delete_stars.o
	$(CXXLD) delete_stars.o -o delete_stars $(LIB_DIR) $(ALL_LIBS)

check_correlate: $(OBJECT_CHECK)
	$(CXXLD) $(OBJECT_CHECK) -o check_correlate $(LIB_DIR) $(ALL_LIBS)

add_star: add_star.o
	$(CXXLD) add_star.o -o add_star $(LIB_DIR) $(ALL_LIBS)

analyze: analyze.o
	$(CXXLD) analyze.o -o analyze $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/analyze $(BIN_DIR)/analyze

analyze_bvri: analyze_bvri.o trans_coef.o
	$(CXXLD) analyze_bvri.o trans_coef.o -o analyze_bvri $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/analyze_bvri $(BIN_DIR)/analyze_bvri

prep_for_pinpoint: prep_for_pinpoint.o
	$(CXXLD) prep_for_pinpoint.o -o prep_for_pinpoint $(LIB_DIR) $(ALL_LIBS)

star_match.o: correlate.h aperture_phot.h
correlate1.o: correlate_internal1.h correlate.h high_precision.h
aperture_phot.o: aperture_phot.h
high_precision.o: high_precision.h correlate_internal1.h

include ../astro.prog.mk
