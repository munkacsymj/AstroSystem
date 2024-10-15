TARGETS = goto star_sync focus_star park unpark 
all: $(TARGETS)

goto: goto.o 
	$(CXXLD) -g goto.o $(LIB_DIR) $(ALL_LIBS)  -o goto
	ln -f -s $(PWD)/goto $(BIN_DIR)/goto

dec_ra_test: dec_ra_test.o 
	$(CXXLD) -g dec_ra_test.o $(LIB_DIR) $(ALL_LIBS)  -o dec_ra_test

focus_star: focus_star.o 
	$(CXXLD) -g focus_star.o $(LIB_DIR) $(ALL_LIBS)  -o focus_star

drift_wait: drift_wait.o 
	$(CXXLD) -g drift_wait.o $(LIB_DIR) $(ALL_LIBS)  -o drift_wait

tweak: tweak.o 
	$(CXXLD) -g tweak.o $(LIB_DIR) $(ALL_LIBS) -o tweak

park: park.o 
	$(CXXLD) -g park.o $(LIB_DIR) $(ALL_LIBS) -o park
	ln -f -s $(PWD)/park $(BIN_DIR)/park

unpark: unpark.o 
	$(CXXLD) -g unpark.o $(LIB_DIR) $(ALL_LIBS) -o unpark
	ln -f -s $(PWD)/unpark $(BIN_DIR)/unpark

star_sync: star_sync.o
	$(CXXLD) -g star_sync.o $(LIB_DIR) $(ALL_LIBS) -o star_sync
	ln -f -s $(PWD)/star_sync $(BIN_DIR)/star_sync

star_sync_test: star_sync_test.o
	$(CXXLD) -g star_sync_test.o $(LIB_DIR) $(ALL_LIBS) -o star_sync_test

star_sync_test1: star_sync_test1.o
	$(CXXLD) -g star_sync_test1.o $(LIB_DIR) $(ALL_LIBS) -o star_sync_test1

goto.o: goto.cc

err_pt: err_pt.o parab1.o
	$(CXXLD) -g err_pt.o parab1.o $(LIB_DIR) $(ALL_LIBS) -o err_pt

summarize_mount_error: summarize_mount_error.o
	$(CXXLD) -g summarize_mount_error.o $(LIB_DIR) $(ALL_LIBS) -o summarize_mount_error
	ln -f -s $(PWD)/summarize_mount_error $(BIN_DIR)/summarize_mount_error

include ../astro.prog.mk
