TARGETS = expose
all: $(TARGETS)

expose: expose.o 
	$(CXXLD) expose.o -o expose $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/expose $(BIN_DIR)/expose

image_profile: image_profile.o
	$(CXXLD) image_profile.o -o image_profile $(LIB_DIR) $(ALL_LIBS)

expose.o: expose.cc

include ../astro.prog.mk

