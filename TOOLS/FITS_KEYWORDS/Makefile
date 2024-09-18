TARGETS     = fits_keywords
OBJECTS	= fits_keywords.o
all: $(TARGETS)

fits_keywords: $(OBJECTS)
	$(CXXLD) -g $(OBJECTS) $(LIB_DIR) $(ALL_LIBS) -o fits_keywords
	ln -sf $(PWD)/fits_keywords $(BIN_DIR)/fits_keywords

include ../astro.prog.mk

