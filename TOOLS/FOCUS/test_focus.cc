#include <stdio.h>
#include <Image.h>
#include <unistd.h>		// sleep()

double aperture_sum(double centerx, double centery, Image &image);

void usage(void) {
  fprintf(stderr, "usage: test_focus -i image.fits\n");
  exit (-2);
}

int main(int argc, char **argv) {
  int ch;
  const char *image_file = 0;

  while((ch = getopt(argc, argv, "i:")) != -1) {
    switch(ch) {
    case 'i':
      image_file = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if (image_file == 0) {
    usage();
  }
  Image this_image(image_file);
  Image *image = &this_image;

  if(image->GetIStarList()->NumStars == 0) {
    fprintf(stderr, "ERROR: no stars found. Giving up.\n");
    exit(-2);
  }

  // Initialize center_x,y to the location of the largest star found
  // by GetIStarList() 
  int largest_star_index = image->LargestStar();
  double center_x =
    image->GetIStarList()->StarCenterX(largest_star_index);
  double center_y =
    image->GetIStarList()->StarCenterY(largest_star_index);
  //const double total_count = image->GetIStarList()->IStarPixelSum(largest_star_index);
  const double background = image->statistics()->StdDev;
  //const double SNR = (total_count/background);
  const double SNR =
    (aperture_sum(center_x, center_y, *image)-image->statistics()->AveragePixel)/background;

  fprintf(stderr, "star center at (%f, %f) with SNR = %.1lf\n", center_x, center_y, SNR);
  return 0;
}

double aperture_sum(double centerx, double centery, Image &image) {
  ImageInfo *info = image.GetImageInfo();
  double pixel_scale = 1.52; // arcsec per pixel default
  if (info && info->CDeltValid()) {
    pixel_scale = info->GetCDelt1();
  }

  
  // 8 arcsecond radius should be plenty
  const double radius = 8.0/pixel_scale;
  const double radius_sq = (radius*radius);
  int pixel_count = 0;
  double sum = 0.0;

  for (int del_y=-radius; del_y<radius; del_y++) {
    for (int del_x=-radius; del_x<radius; del_x++) {
      const double d_sq = (del_x*del_x + del_y*del_y);
      if (d_sq <= radius_sq) {
	sum += image.pixel(del_x+(int)(centerx+0.5),
			   del_y+(int)(centery+0.5));
	pixel_count++;
      }
    }
  }

  return sum/pixel_count;
}
      
