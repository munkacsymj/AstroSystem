TARGETS = dark_manager make_master_bias

all: $(TARGETS)

dark_manager: dark_manager.o
	$(CXXLD) -g dark_manager.o -o dark_manager $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/dark_manager $(BIN_DIR)/dark_manager

darktest: darktest.o
	$(CXXLD) -g darktest.o -o darktest $(LIB_DIR) $(ALL_LIBS)

make_master_bias: make_master_bias.o
	$(CXXLD) -g make_master_bias.o -o make_master_bias $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/make_master_bias $(BIN_DIR)/make_master_bias

include ../astro.prog.mk
