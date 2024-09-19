TARGETS = rebuild_strategy_database
all: $(TARGETS)

rebuild_strategy_database: rebuild_strategy_database.o
	$(CXXLD) rebuild_strategy_database.o -o rebuild_strategy_database $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/rebuild_strategy_database $(BIN_DIR)/rebuild_strategy_database

rebuild_strategy_database.o: rebuild_strategy_database.cc

include ../astro.prog.mk
