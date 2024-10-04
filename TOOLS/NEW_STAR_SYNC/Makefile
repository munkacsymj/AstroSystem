ifeq ($(GM2000),GM2000)
  TARGETS = gm2000_sync
else
  TARGETS =
endif 

all: $(TARGETS)

star_sync: star_sync.o
	$(CXXLD) -g star_sync.o $(LIB_DIR) $(ALL_LIBS) -o star_sync
	ln -f -s $(PWD)/star_sync $(BIN_DIR)/star_sync

gm2000_sync: gm2000_sync.o
	$(CXXLD) -g gm2000_sync.o $(LIB_DIR) $(ALL_LIBS) -o gm2000_sync
	ln -f -s $(PWD)/gm2000_sync $(BIN_DIR)/gm2000_sync

sync_test: sync_test.o
	$(CXXLD) -g sync_test.o $(LIB_DIR) $(ALL_LIBS) -o sync_test

gemini_test: gemini_test.o
	$(CXXLD) -g gemini_test.o $(LIB_DIR) $(ALL_LIBS) -o gemini_test

include ../astro.prog.mk
