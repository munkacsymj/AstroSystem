TARGETS = iexpand numexpand
all: $(TARGETS)

iexpand: iexpand.o 
	$(CXXLD) iexpand.o -o iexpand $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/iexpand $(BIN_DIR)/iexpand

numexpand: numexpand.o 
	$(CXXLD) numexpand.o -o numexpand $(LIB_DIR) $(ALL_LIBS)
	ln -s -f $(PWD)/numexpand $(BIN_DIR)/numexpand

iexpand.o: iexpand.cc
numexpand.o: numexpand.cc

include ../astro.prog.mk

