TARGETS = image_monitor
all: $(TARGETS)

XLIBS = -lXm -lXt 

image_monitor: image_monitor.o
	$(CXXLD) -g -rdynamic image_monitor.o $(XLIBS) $(LIB_DIR) $(ALL_LIBS) `pkg-config --libs gtk+-3.0` -o image_monitor
	ln -sf $(PWD)/image_monitor $(BIN_DIR)/image_monitor

image_monitor.o: image_monitor.cc
	g++ -O0 -g -rdynamic $(CXXFLAGS) `pkg-config --cflags gtk+-3.0` -c image_monitor.cc 

include ../astro.prog.mk
