TARGETS     = print_corr
OBJECTS	= print_corr.o
all: $(TARGETS)

print_corr: $(OBJECTS)
	$(CXXLD) -g $(OBJECTS) $(LIB_DIR) $(ALL_LIBS) -o print_corr

include ../astro.prog.mk

