TARGETS = notify notify_test
all: $(TARGETS)

notify: notify.o 
	$(CXXLD) -g notify.o $(LIB_DIR) $(ALL_LIBS)  -o notify
	ln -f -s $(PWD)/notify $(BIN_DIR)/notify

notify_test: notify_test.o
	$(CXXLD) -g notify_test.o $(LIB_DIR) $(ALL_LIBS)  -o notify_test

include ../astro.prog.mk
