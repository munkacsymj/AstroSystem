OBJECTS = simple_session.o
TARGETS = simple_session  
all: $(TARGETS)

simple_session: simple_session.o
	$(CXXLD) -g simple_session.o $(LIB_DIR) $(ALL_LIBS) -o simple_session
	ln -s -f $(PWD)/simple_session $(BIN_DIR)/simple_session

testit: testit.o 
	$(CXXLD) -g testit.o $(LIB_DIR) $(ALL_LIBS) -o testit

testit1: testit1.o 
	$(CXXLD) -g testit1.o $(LIB_DIR) $(ALL_LIBS) -o testit1

obs_test: obs_test.o obs_spreadsheet.o
	$(CXXLD) -g  obs_test.o obs_spreadsheet.o $(LIB_DIR) $(ALL_LIBS) -o obs_test

include ../astro.prog.mk
