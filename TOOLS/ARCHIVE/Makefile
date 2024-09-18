TARGETS =  smart_add_archive smart_analyze_archive
all: $(TARGETS)

smart_add_archive: smart_add_archive.o
	$(CXXLD) smart_add_archive.o -o smart_add_archive

smart_analyze_archive: smart_analyze_archive.o
	$(CXXLD) smart_analyze_archive.o -o smart_analyze_archive

include ../astro.prog.mk

