TARGETS = offline test_circle_box find_match test_model graph_composite_profile analyze_composite sliding_focus_model test_blur image_to_csv 

OBJECTS = offline.o

FIND_MATCH_OBJECTS = find_match.o \
	residuals.o \
	estimate_params.o \
	build_ref_image.o \
	circle_box.o \
	gaussian_blur.o

TEST_MODEL_OBJECTS = test_model.o \
	residuals.o \
	estimate_params.o \
	build_ref_image.o \
	circle_box.o \
	gaussian_blur.o

CXXFLAGS = $(CXXFLAGS)

all: $(TARGETS)

sliding_focus_model: sliding_focus_model.o
	$(CXXLD) -g sliding_focus_model.o $(LIB_DIR) $(ALL_LIBS) -o sliding_focus_model

analyze_composite: analyze_composite.o gaussian_fit.o
	$(CXXLD) -g analyze_composite.o gaussian_fit.o $(LIB_DIR) $(ALL_LIBS) -o analyze_composite

analyze_composite_alt: analyze_composite_alt.o gaussian_fit.o
	$(CXXLD) -g analyze_composite_alt.o gaussian_fit.o $(LIB_DIR) $(ALL_LIBS) -o analyze_composite_alt

graph_composite_profile: graph_composite_profile.o
	$(CXXLD) -g graph_composite_profile.o $(LIB_DIR) $(ALL_LIBS) -o graph_composite_profile

find_match: $(FIND_MATCH_OBJECTS)
	$(CXXLD) -g $(FIND_MATCH_OBJECTS) $(LIB_DIR) $(ALL_LIBS) -lgsl -lgslcblas -o find_match
	ln -sf $(PWD)/find_match $(BIN_DIR)/find_match

test_model: $(TEST_MODEL_OBJECTS)
	$(CXXLD) -g $(TEST_MODEL_OBJECTS) $(LIB_DIR) $(ALL_LIBS) -lgsl -lgslcblas -o test_model

offline: $(OBJECTS)
	$(CXXLD) -g $(OBJECTS) $(LIB_DIR) -lgsl -lgslcblas -lm -o offline

test_circle_box: circle_box.o test_circle_box.o build_ref_image.o gaussian_blur.o
	$(CXXLD) -g circle_box.o test_circle_box.o build_ref_image.o gaussian_blur.o $(LIB_DIR) $(ALL_LIBS) -o test_circle_box

test_blur: circle_box.o test_blur.o build_ref_image.o gaussian_blur.o estimate_params.o
	$(CXXLD) -g circle_box.o test_blur.o build_ref_image.o gaussian_blur.o estimate_params.o $(LIB_DIR) $(ALL_LIBS) -o test_blur

image_to_csv: image_to_csv.o
	$(CXXLD) -g image_to_csv.o $(LIB_DIR) $(ALL_LIBS) -o image_to_csv

include ../astro.prog.mk

