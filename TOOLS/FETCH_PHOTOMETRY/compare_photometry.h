/* This may look like C code, but it is really -*-c++-*- */
#include "aavso_photometry.h"

// These are return values from compare_photometry

#define COMPARE_MATCHES 0
#define COMPARE_MISMATCH 1

// If update_flag is true, will actually modify the HGSC in-memory
// data to match the AAVSO photometry. 
int compare_photometry(PhotometryRecord *data, // one line of AAVSO photometry
		       HGSCList *catalog,
		       int update_flag);

// This returns the string giving the name of the photometry sequence
// that is being used 
const char *sequence_name(void);
