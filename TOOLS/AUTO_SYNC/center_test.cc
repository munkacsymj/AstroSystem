#include <stdio.h>
#include "dec_ra.h"
#include "Image.h"

int main(int argc, char **argv) {
  {
    const char *filename = "/home/IMAGES/4-6-2020/image057.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }
  {
    const char *filename = "/home/IMAGES/4-6-2020/image058.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image061.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image063.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image064.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image065.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image058.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image066.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image067.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image069.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image071.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image074.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image075.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image076.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image077.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image078.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image079.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image080.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image081.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image082.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

  {
    const char *filename = "/home/IMAGES/4-6-2020/image084.fits";
    Image image(filename);
    int status;
    DEC_RA current_center = image.ImageCenter(status);
    //bool scope_on_west = true;
    EPOCH j2000(2000);
    DEC_RA true_plate_center = ToEpoch(current_center, j2000, EpochOfToday());
    fprintf(stdout, "%s: %s   %s\n",
	    filename,
	    true_plate_center.string_longra_of(),
	    true_plate_center.string_fulldec_of());
  }

}
