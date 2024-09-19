TARGETS =  flip mount # show_mount_model check_alignment star_sync_test
all: $(TARGETS)

show_mount_model: show_mount_model.o
	$(CXXLD) show_mount_model.o -o show_mount_model $(LIB_DIR) $(ALL_LIBS)

show_mount_model.o: show_mount_model.cc

check_alignment: check_alignment.o
	$(CXXLD) check_alignment.o -o check_alignment $(LIB_DIR) $(ALL_LIBS)

mount: mount.o
	$(CXXLD) mount.o -o mount $(LIB_DIR) $(ALL_LIBS)

flip: flip.o
	$(CXXLD) flip.o -o flip $(LIB_DIR) $(ALL_LIBS)

check_alignment.o: check_alignment.cc

star_sync_test: star_sync_test.o mount_model.o
	$(CXXLD) star_sync_test.o mount_model.o -o star_sync_test $(LIB_DIR) $(ALL_LIBS)

star_sync_test.o: star_sync_test.cc mount_model.h

include ../astro.prog.mk
