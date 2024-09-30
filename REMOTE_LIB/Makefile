all: libremote_lib.a libremote_lib.so list_image_profiles

clean:
	rm -f libremote_lib.a libremote_lib.so *.o

ASTRO_ENV=INSIDE
#ASTRO_ENV=OUTSIDE
OPT_TARGETS=

#MOUNT:="Gemini" # alternatively, GM2000
MOUNT:="GM2000" # alternatively, "Gemini"
ASTRO_DEFS = # -DGEMINI # alternatively, -DGM2000

#HOSTNAME=JELLYBEAN
HOSTNAME=NewWorkshop

ifeq ($(HOSTNAME),JELLYBEAN)
  HOSTDEF= -D JELLYBEAN
else
  HOSTDEF= -D POGO
endif

ifeq ($(INTERFACE),INDI)
  INTERFACE_DEF= -D INDI -D INTERFACE=INDI
  INTERFACE_FILES = astro_indi.o blocker_indi.o cfw_indi.o cooler_indi.o focuser_indi.o mount_indi.o \
	scope_api_indi.o st10xme_indi.o camera_api_indi.o
else ifeq($(INTERFACE),NATIVE)
  INTERFACE_DEF= -D INTERFACE=NATIVE
  INTERFACE_FILES = scope_api.o camera_api.o
else
  $(error Environment variable INTERFACE not defined as INDI or NATIVE)
endif

ASTROHOME=$(HOME)

ifeq ($(strip $(HOSTNAME)),JELLYBEAN)
  CFITSIO=$(ASTROHOME)/CFITSIO/cfitsio
else
  CFITSIO=$(ASTROHOME)/ASTRO/CFITSIO/cfitsio
endif

ifeq ($(strip $(MOUNT)),"Gemini")
  MOUNT_TARGETS= mount_model.o mount_model_int.o gemini_messages.o
else
  MOUNT_TARGETS= 
endif

CXXFLAGS = -O3 -Wall -g -fPIC \
	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
	$(HOSTDEF) \
	$(ASTRO_DEFS) \
	$(INTERFACE_DEF) \
	-I . \
	-I ../IMAGE_LIB \
	-I ../DATA_LIB \
	-I ../ASTRO_LIB \
	-I $(CFITSIO) \
	-I /usr/X11R6/include


visibility.o:		visibility.h	       alt_az.h        julian.h
mount_model.o:          mount_model.h 	       julian.h        dec_ra.h
image_notify.o:		image_notify.h
scope_api.o:            scope_api.h	       mount_model.h
camera_api.o:           camera_api.h
TrackerData.o:          TrackerData.h
FilterDataMessage.o:    FilterDataMessage.h    gen_message.h
FilterQueryMessage.o:   FilterQueryMessage.h   gen_message.h
CoolerMessage.o:	CoolerMessage.h        gen_message.h
ExposeMessage.o:        ExposeMessage.h        gen_message.h
FITSMessage.o:          FITSMessage.h          gen_message.h
RequestStatusMessage.o: RequestStatusMessage.h gen_message.h
StatusMessage.o:        StatusMessage.h        gen_message.h
gen_message.o:          gen_message.h          \
                        RequestStatusMessage.h StatusMessage.h \
                        FITSMessage.h	       

lx_FlatLightMessage.o:  lx_FlatLightMessage.h  lx_gen_message.h
lx_FocusMessage.o:      lx_FocusMessage.h      lx_gen_message.h
lx_RequestStatusMessage.o: lx_RequestStatusMessage.h lx_gen_message.h
lx_StatusMessage.o:     lx_StatusMessage.h     lx_gen_message.h
lx_gen_message.o:       lx_gen_message.h       lx_StatusMessage.h \
                        lx_RequestStatusMessage.h \
			lx_TrackMessage.h
lx_TrackMessage.o:      lx_TrackMessage.h      lx_gen_message.h
lx_ResyncMessage.o:     lx_ResyncMessage.h     lx_gen_message.h
lx_ScopeMessage.o:      lx_ScopeMessage.h      lx_gen_message.h
lx_ScopeResponseMessage.o: lx_ScopeResponseMessage.h lx_gen_message.h
alt_az.o:               alt_az.h julian.h dec_ra.h
dec_ra.o:               dec_ra.h julian.h
julian.o:               julian.h
gemini_messages.o:      dec_ra.h julian.h gemini_messages.h
drifter.o:              drifter.h dec_ra.h julian.h scope_api.h

list_image_profiles: list_image_profiles.o ./image_profile.o ../DATA_LIB/json.o
	g++ -o list_image_profiles list_image_profiles.o ./image_profile.o ../DATA_LIB/json.o


ifeq ($(ASTRO_ENV),INSIDE)
OPT_TARGETS = scope_api.o \
	$(MOUNT_TARGETS) \
	image_notify.o \
	camera_api.o \
	visibility.o
endif

TARGETS = FITSMessage.o   \
          RequestStatusMessage.o \
	 camera_message.o \
	image_profile.o \
          StatusMessage.o \
	 drifter.o \
          gen_message.o \
	 lx_FlatLightMessage.o \
          lx_FocusMessage.o \
          lx_RequestStatusMessage.o \
	  lx_ScopeResponseMessage.o \
                 lx_StatusMessage.o \
		 lx_ScopeMessage.o \
		 lx_TrackMessage.o \
	         lx_ResyncMessage.o \
                 lx_gen_message.o \
		 alt_az.o \
		 dec_ra.o \
		 refraction.o \
		 sync_session.o \
		 TrackerData.o \
		 julian.o \
		 $(OPT_TARGETS) \
	$(INTERFACE_FILES) 

libremote_lib.o: $(TARGETS)
	ld -r -o libremote_lib.o $(TARGETS)

libremote_lib.a: libremote_lib.o
	ar -r libremote_lib.a libremote_lib.o
	ranlib libremote_lib.a

libremote_lib.so: libremote_lib.o
	g++ -shared -o libremote_lib.so libremote_lib.o -L/usr/X11R6/lib -lXaw

test_refraction: test_refraction.o refraction.o libremote_lib.o
	g++ -o test_refraction test_refraction.o ../ASTRO_LIB/libastro.o -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -L/usr/X11R6/lib -lXaw -lXt -lX11 -lcfitsio -lgsl  -lgslcblas -lm

test_harness: test_harness.o camera_message.o
	g++ -o test_harness test_harness.o camera_message.o ../ASTRO_LIB/libastro.o -L $(ASTROHOME)/ASTRO/CFITSIO/cfitsio -L/usr/X11R6/lib -lXaw -lXt -lX11 -lcfitsio -lgsl  -lgslcblas -lm -lrt

