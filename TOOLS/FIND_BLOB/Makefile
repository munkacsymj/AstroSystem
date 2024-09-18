TARGETS = find_blob
all: $(TARGETS)

find_blob: find_blob.o
	$(CXXLD) find_blob.o -o find_blob $(LIB_DIR) $(ALL_LIBS)
	ln -f -s $(PWD)/find_blob $(BIN_DIR)/find_blob

find_blob.o: find_blob.cc

include ../astro.prog.mk
