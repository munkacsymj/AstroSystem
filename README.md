# Summary

This is a software system that supports automated telescope imaging in support of stellar photometry. At its top-most level, the following major elements exist:

# A Common Library:

The common library includes both Python and C++ pieces; the C++ is far more complete. The Python library modules are in the subdirectory named PYTHON_LIB. The C++ library is split into 4 major pieces:
    
## IMAGE_LIB: 
a library that defines classes to support images, starlists, and coordinate system transformations between pixel coordinates and sky coordinates
        
## REMOTE_LIB: 
a library that defines an interface API to the observatory's hardware: telescopes, focusers, cameras, filter wheels
        
## SESSION_LIB: 
a library that provides overall coordination and control of the observatory: scheduling, exposure management, focus management, session-level guiding
        
## DATA_LIB: 
a library that provides data-product-related classes: the Guide Star Catalog, bright star lists, the current observatory configuration, and the common libraries key internal databases: dbase and astro_db
        
The four pieces are integrated together into a single library .so file in ASTRO_LIB (which also contains a single important .h file that provides key file pathnames)
    
# Servers (effectively device drivers) that run in the observatory:

## CCD_SERVER: 
a server that runs in the observatory to control the camera, cooler, and filter wheel.
    
## SCOPE_SERVER: 
a server that controls the mount and focuser
    
# Executable programs and tools
These all reside in a directory named "TOOLS". Almost all of these are built upon the Common Library. There are currently about 70 subfolders to TOOLS. Each contains one or more executable programs.

# Prerequisites

This is (still) kind of messy, but you need:

* IRAF (this is only used for one program: photometry. AstroImageJ is a fine substitute and eliminates all of the hassle associated with installing IRAF)
* cfitsio (a NASA project, available through heasarc.gsfc.masa.gov/fitsio)
* X11
* INDI (including all the development tools needed to compile INDI clients: libindiclient.*)
* Misc libraries: ceres, lpthread, rt, glog, gsl, gslcblas, m

# Environment Variables

In order to compile, you need to set the environment variable INTERFACE to either INDI or NATIVE. (If you don't set this variable, you'll get a pretty immediate error message)

# Compiling

Within the root directory of the system you're building (e.g., "/something/AstroSystem"), create the empty directory "BIN" to hold the final executables.

Type "make all"

