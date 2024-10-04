TARGETS = auto_sync is_flipped test_dec center_test
ifeq ($(GM2000),GM2000)
  TARGETS = $(TARGETS) list_align_gm2000
endif

all: $(TARGETS)

center_test: center_test.o
	$(CXXLD) center_test.o -o center_test $(LIB_DIR) $(ALL_LIBS) 

test_dec: test_dec.o
	$(CXXLD) test_dec.o -o test_dec $(LIB_DIR) $(ALL_LIBS) 

auto_sync: auto_sync.o
	$(CXXLD) auto_sync.o -o auto_sync $(LIB_DIR) $(ALL_LIBS) 
	ln -sf $(PWD)/auto_sync $(BIN_DIR)/auto_sync

auto_sync_gm2000: auto_sync_gm2000.o
	$(CXXLD) auto_sync_gm2000.o -o auto_sync_gm2000 $(LIB_DIR) $(ALL_LIBS) 
	ln -sf $(PWD)/auto_sync_gm2000 $(BIN_DIR)/auto_sync_gm2000

list_align_gm2000: list_align_gm2000.o
	$(CXXLD) list_align_gm2000.o -o list_align_gm2000 $(LIB_DIR) $(ALL_LIBS) 
	ln -sf $(PWD)/list_align_gm2000 $(BIN_DIR)/list_align_gm2000

is_flipped: is_flipped.o
	$(CXXLD) is_flipped.o -o is_flipped $(LIB_DIR) $(ALL_LIBS) 
	ln -sf $(PWD)/is_flipped $(BIN_DIR)/is_flipped

include ../astro.prog.mk
