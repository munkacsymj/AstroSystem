OPTIMIZE= -O0

OBJECTS= trans_coef.o

TARGETS =  analyze_bvri bvri_pretty test_colors bvri_report image_to_tg update_bvri_db delete_bvri_entry

EXTRA_INCLUDES = -I ../STAR_MATCH

all: $(TARGETS)

update_bvri_db: update_bvri_db.o
	$(CXXLD) update_bvri_db.o -o update_bvri_db $(LIB_DIR) $(ALL_LIBS) 
	ln -sf $(PWD)/update_bvri_db $(BIN_DIR)/update_bvri_db

delete_bvri_entry: delete_bvri_entry.o
	$(CXXLD) delete_bvri_entry.o -o delete_bvri_entry $(LIB_DIR) $(ALL_LIBS) 
	ln -sf $(PWD)/delete_bvri_entry $(BIN_DIR)/delete_bvri_entry

image_to_tg: image_to_tg.o
	$(CXXLD) image_to_tg.o -o image_to_tg $(LIB_DIR) $(ALL_LIBS) 
	ln -sf $(PWD)/image_to_tg $(BIN_DIR)/image_to_tg

analyze_bvri: analyze_bvri.o trans_coef.o colors.o
	$(CXXLD) analyze_bvri.o trans_coef.o colors.o -o analyze_bvri $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/analyze_bvri $(BIN_DIR)/analyze_bvri

bvri_pretty: bvri_pretty.o
	$(CXXLD) bvri_pretty.o -o bvri_pretty $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/bvri_pretty $(BIN_DIR)/bvri_pretty

bvri_report: bvri_report.o trans_coef.o colors.o groups.o 
	$(CXXLD) bvri_report.o trans_coef.o colors.o groups.o -o bvri_report $(LIB_DIR) $(ALL_LIBS)
	ln -sf $(PWD)/bvri_report $(BIN_DIR)/bvri_report

test_colors: test_colors.o colors.o trans_coef.o
	$(CXXLD) test_colors.o colors.o trans_coef.o -o test_colors $(LIB_DIR) $(ALL_LIBS)

include ../astro.prog.mk


