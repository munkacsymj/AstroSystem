MAKE=make -j 10
SUBDIRS = DATA_LIB IMAGE_LIB REMOTE_LIB ASTRO_LIB SESSION_LIB TOOLS

all:
	(cd SESSION_LIB && $(MAKE) libs)
	list='$(SUBDIRS)'; for subdir in $$list; do \
	test "$$subdir" = . || (cd $$subdir && $(MAKE) all) ; \
	done

clean:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	test "$$subdir" = . || (cd $$subdir && $(MAKE) clean) ; \
	done
