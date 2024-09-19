TARGETS = measure_linearity analyze_linearity
all: $(TARGETS)

measure_linearity: measure_linearity.o 
	$(CXXLD) -g measure_linearity.o $(LIB_DIR) $(ALL_LIBS)  -o measure_linearity
	ln -f -s $(PWD)/measure_linearity $(BIN_DIR)/measure_linearity

analyze_linearity: analyze_linearity.o 
	$(CXXLD) -g analyze_linearity.o $(LIB_DIR) $(ALL_LIBS)  -o analyze_linearity
	ln -f -s $(PWD)/analyze_linearity $(BIN_DIR)/analyze_linearity

new_analyze: new_analyze.o 
	$(CXXLD) -g new_analyze.o $(LIB_DIR) $(ALL_LIBS)  -o new_analyze -lstdc++fs

test_cc: test_cc.o
	$(CXXLD) -g test_cc.o $(LIB_DIR) $(ALL_LIBS) -o test_cc -lstdc++fs

OPTIMIZE = -O0

include ../astro.prog.mk
