TARGETS = worker

all: $(TARGETS)

worker: worker.o
	$(CXXLD) -g worker.o -o worker $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/worker $(BIN_DIR)/worker

include ../astro.prog.mk
