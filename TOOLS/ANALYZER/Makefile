TARGETS = set_directive do_bvri analyzer
all: $(TARGETS)

ANALY_MODULES = \
	dnode.o analyzer.o colors.o trans_coef.o

analyzer: $(ANALY_MODULES)
	$(CXXLD) -g $(ANALY_MODULES) $(LIB_DIR) $(ALL_LIBS) -o analyzer
	ln -f -s $(PWD)/analyzer $(BIN_DIR)/analyzer

set_directive: set_directive.o 
	$(CXXLD) -g set_directive.o $(LIB_DIR) $(ALL_LIBS)  -o set_directive
	ln -f -s $(PWD)/set_directive $(BIN_DIR)/set_directive

do_bvri: do_bvri.py
	ln -f -s $(PWD)/do_bvri.py $(BIN_DIR)/do_bvri

include ../astro.prog.mk
