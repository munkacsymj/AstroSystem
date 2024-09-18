TARGETS = print_star make_composite  focus_measure  \
	 nlls_measure1 new_focus_measure
all: $(TARGETS)

XLIBS = -lXm

print_star: print_star.o 
	$(CXXLD) -g print_star.o $(LIB_DIR) $(ALL_LIBS) -o print_star

make_composite: make_composite.o 
	$(CXXLD) -g make_composite.o $(LIB_DIR) $(ALL_LIBS) -o make_composite
	ln -f -s $(PWD)/make_composite $(BIN_DIR)/make_composite

focus_alg_alt: focus_alg_alt.o focus_alg_alt_test.o
	$(CXXLD) -g focus_alg_alt.o $(LIB_DIR) $(ALL_LIBS) -o focus_alg_alt

focus_measure: focus_measure.o 
	$(CXXLD) -g focus_measure.o $(LIB_DIR) $(ALL_LIBS) -o focus_measure

nlls_measure: nlls_measure.o nlls.o
	$(CXXLD) -g nlls_measure.o $(LIB_DIR) $(ALL_LIBS) -o nlls_measure

nlls_measure_old: nlls_measure.o nlls_old.o
	$(CXXLD) -g nlls_measure_old.o $(LIB_DIR) $(ALL_LIBS) -o nlls_measure_old

focus_alg_test: focus_alg_test.o scope_model.o focus_alg.o
	$(CXXLD) -g focus_alg_test.o $(LIB_DIR) $(ALL_LIBS) -o focus_alg_test

nlls_measure1: nlls_measure1.o nlls_simple.o
	$(CXXLD) -g nlls_measure1.o nlls_simple.o $(LIB_DIR) $(ALL_LIBS) -o nlls_measure1

new_focus_measure: new_focus_measure.o aperture_phot.o
	$(CXXLD) -g new_focus_measure.o aperture_phot.o $(LIB_DIR) $(ALL_LIBS) -o new_focus_measure

include ../astro.prog.mk
