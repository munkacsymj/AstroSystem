/* This may look like C code, but it is really -*-c++-*- */

// This is the location of the Image files
#define IMAGE_DIR "/home/IMAGES"
#define TMP_IMAGE_DIR "/home/IMAGES/tmp"

// This is the name of the computer outside
#define JELLYBEAN_HOSTNAME "jellybean2"
//#define JELLYBEAN_HOSTNAME "jellybean"

// This is the location of the mount model
#define MOUNT_MODEL_DIR  "/home/ASTRO/CURRENT_DATA"
#define MOUNT_MODEL_FILE "/home/ASTRO/CURRENT_DATA/mount_model.data"

// Where the bad_pixel file is located
#define BAD_PIXEL_FILE "/home/ASTRO/CURRENT_DATA/bad_pixel_map.txt"

// Where the coordinate system transformation cache is kept
#define TRANSFORM_DIR "/home/ASTRO/CURRENT_DATA"

//This is the location of the strategy files
#define STRATEGY_DIR "/home/ASTRO/STRATEGIES"

// This is the location of most commands
#define COMMAND_DIR "/home/mark/ASTRO/BIN"

// Where the obs_record database is maintained
#define OBS_RECORD_FILENAME "/home/ASTRO/OBSERVATIONS/observations"

// Where iraf is run from
#define IRAF_ROOT "/home/mark/iraf0"

// Where the AAVSO validation file is kept
#define AAVSO_VALIDATION_DIR "/home/ASTRO/REF_DATA/STARS"

// Where the catalog files are kept
#define CATALOG_DIR "/home/ASTRO/CATALOGS"

// Where the bright star files are kept
#define BRIGHT_STAR_DIR "/home/ASTRO/REF_DATA"

// Where the HGSC catalog is kept
#define HGSC_CATALOG_DIR "/home/ASTRO/REF_DATA/HGSC"

// Where misc reference data is kept
#define REF_DATA_DIR "/home/ASTRO/REF_DATA"

// Where archive.dat is kept, holding archived observations
#define ARCHIVE_DIR "/home/ASTRO/ARCHIVE"

// Where the default filter information is kept
#define FILTER_DEFAULT_FILE "/home/ASTRO/CURRENT_DATA/default_filter.txt"

#define INTERNAL_PRECESSION // INTERNAL means in this software
#define EXTERNAL_REFRACTION
#define EXTERNAL_MOUNT_MODEL

#undef EXTERNAL_PRECESSION // EXTERNAL means in the mount
#undef INTERNAL_REFRACTION
#undef INTERNAL_MOUNT_MODEL

#undef GEMINI
#define GM2000
#undef LX200






