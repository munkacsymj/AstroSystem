TARGETS = test_obs_record import_bvri
all: $(TARGETS)

test_obs_record: test_obs_record.o
	$(CXXLD) test_obs_record.o -o test_obs_record $(LIB_DIR) $(ALL_LIBS)

import_bvri: import_bvri.o
	$(CXXLD) import_bvri.o -o import_bvri $(LIB_DIR) $(ALL_LIBS)

test_obs_record.o: test_obs_record.cc
import_bvri.o: import_bvri.cc

include ../astro.prog.mk
