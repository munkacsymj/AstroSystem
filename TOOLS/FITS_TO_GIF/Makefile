TARGETS = fits_to_gif #fits_to_png
OBJECTS = fits_to_gif.o

all: $(TARGETS)

include ../astro.prog.mk

#ALL_LIBS = -lGraphicsMagick++ -lGraphicsMagick -ljbig -llcms2 -ltiff -lfreetype -ljpeg -lwmflite -lXext -lSM -lICE -lX11 -llzma -lbz2 -lxml2 -lz -lm -lgomp -lpthread -lastro -lcfitsio -lgsl -lgslcblas -lXt -lX11 -lXaw -lm -lrt

fits_to_png.o: fits_to_png.cc
	$(CXX) -g -c $(CXXFLAGS) -I /usr/include/GraphicsMagick -fno-strict-aliasing -pthread fits_to_png.cc

fits_to_png: fits_to_png.o
	$(CXXLD) -g fits_to_png.o $(LIB_DIR) -o fits_to_png $(ALL_LIBS)
	ln -sf $(PWD)/fits_to_png $(BIN_DIR)/fits_to_png

fits_to_gif: $(OBJECTS)
	$(CXXLD) -g $(OBJECTS) $(LIB_DIR)  -o fits_to_gif  $(ALL_LIBS)
	ln -sf $(PWD)/fits_to_gif $(BIN_DIR)/fits_to_gif



