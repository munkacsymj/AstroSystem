ifeq ($(GM2000),GM2000)
  TARGETS = scope_status # scope_monitor
else
  TARGETS =
endif

all: $(TARGETS)

XLIBS = -lXm

scope_status: scope_status.o 
	$(CXXLD) -g scope_status.o $(LIB_DIR) $(ALL_LIBS) -o scope_status

scope_monitor: scope_monitor.o scope_interface.o
	$(CXXLD) -g scope_monitor.o scope_interface.o $(LIB_DIR) $(ALL_LIBS) $(XLIBS) -o scope_monitor

include ../astro.prog.mk
