TARGETS = scheduler
all: $(TARGETS)

scheduler: sched_main.o
	$(CXXLD) sched_main.o -o scheduler $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/scheduler $(BIN_DIR)/scheduler

sched_main.o: sched_main.cc

include ../astro.prog.mk

