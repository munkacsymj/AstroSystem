OBJECTS = fix_decra.o
TARGETS = fix_decra
all: $(TARGETS)

OPTIMIZE = -O0

fix_decra: fix_decra.o
	$(CXXLD) -g fix_decra.o $(LIB_DIR) $(ALL_LIBS) -o fix_decra
	ln -s -f $(PWD)/fix_decra $(BIN_DIR)/fix_decra

include ../astro.prog.mk
