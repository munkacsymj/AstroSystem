TARGETS =  import_vphot prep_for_vphot
all: $(TARGETS)

import_vphot: import_vphot.o
	$(CXXLD) import_vphot.o -o import_vphot $(LIB_DIR) $(ALL_LIBS)

prep_for_vphot: prep_for_vphot.o
	$(CXXLD) prep_for_vphot.o -o prep_for_vphot $(LIB_DIR) $(ALL_LIBS)
	ln -f -s $(PWD)/prep_for_vphot $(BIN_DIR)/prep_for_vphot

include ../astro.prog.mk
