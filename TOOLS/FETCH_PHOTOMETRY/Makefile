TARGETS = fetch_photometry fetch_test

all: $(TARGETS)

OBJECTS = fetch_photometry.o \
	compare_photometry.o \
	aavso_photometry.o 

fetch_photometry: $(OBJECTS)
	$(CXXLD) -g $(OBJECTS) $(LIB_DIR) $(ALL_LIBS) -lcurl -o fetch_photometry
	ln -f -s $(PWD)/fetch_photometry $(BIN_DIR)/fetch_photometry

fetch_test: fetch_test.o compare_photometry.o aavso_photometry.o
	$(CXXLD) -g -o $@ $^ $(LIB_DIR) $(ALL_LIBS) -lcurl
	ln -fs $(PWD)/$(@F) $(BIN_DIR)/$(@F)

include ../astro.prog.mk
