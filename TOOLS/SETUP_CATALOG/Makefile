TARGETS =  setup_new_catalog merge_catalogs dec_ra_to_rad
all: $(TARGETS)

setup_new_catalog: setup_new_catalog.o
	$(CXXLD) setup_new_catalog.o $(LIB_DIR) -o setup_new_catalog $(ALL_LIBS)
	ln -s -f $(PWD)/setup_new_catalog $(BIN_DIR)/setup_new_catalog

merge_catalogs: merge_catalogs.o
	$(CXXLD) merge_catalogs.o $(LIB_DIR) -o merge_catalogs $(ALL_LIBS)
	ln -s -f $(PWD)/merge_catalogs $(BIN_DIR)/merge_catalogs

dec_ra_to_rad: dec_ra_to_rad.o
	$(CXXLD) dec_ra_to_rad.o $(LIB_DIR) -o dec_ra_to_rad $(ALL_LIBS)
	ln -s -f $(PWD)/dec_ra_to_rad $(BIN_DIR)/dec_ra_to_rad

setup_catalog.o: HGSC.h
HGSC.o:          HGSC.h

include ../astro.prog.mk
