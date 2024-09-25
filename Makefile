MAKE=make -j 4  
SUBDIRS = DATA_LIB IMAGE_LIB REMOTE_LIB SESSION_LIB ASTRO_LIB TOOLS

all:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	test "$$subdir" = . || (cd $$subdir && $(MAKE) all) ; \
	done

clean:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	test "$$subdir" = . || (cd $$subdir && $(MAKE) clean) ; \
	done
