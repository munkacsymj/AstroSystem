TARGETS = bincrop

OPTIMIZE = -O0

all: $(TARGETS)

bincrop: bincrop.o
	$(CXXLD) bincrop.o -o bincrop $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/bincrop $(BIN_DIR)/bincrop


include ../astro.prog.mk
