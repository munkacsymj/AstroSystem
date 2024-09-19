#include <stdio.h>
#include <Image.h>

double GetAverage(Image &i, int span, int left, int bottom) {
  int count = 0;
  double sum = 0.0;
  for (int x=left; x<left+span; x++) {
    for (int y=bottom; y<bottom+span; y++) {
      sum += i.pixel(x,y);
      count++;
    }
  }

  return sum/count;
}


int main(int argc, char **argv) {
  Image i_206("/home/IMAGES/5-10-2022/image206.fits");
  Image i_dark("/home/IMAGES/5-10-2022/dark10.fits");
  Image i_flat("/home/IMAGES/5-10-2022/flat_Vc.fits");

  Image flat_binned("/tmp/binned.fits");
  Image flat_subimage("/tmp/subimage.fits");

  Image final("/tmp/image.fits");

  fprintf(stderr, "i_206: corner = %lf, center = %lf\n",
	  GetAverage(i_206, 4, 0, 1035),
	  GetAverage(i_206, 4, 633, 520));

  fprintf(stderr, "i_dark: corner = %lf, center = %lf\n",
	  GetAverage(i_dark, 16, 600, 4140),
	  GetAverage(i_dark, 16, 3132, 2080));

  fprintf(stderr, "i_flat: corner = %lf, center = %lf\n",
	  GetAverage(i_flat, 16, 600, 4140),
	  GetAverage(i_flat, 16, 3132, 2080));

  fprintf(stderr, "flat_subimage: corner = %lf, center = %lf\n",
	  GetAverage(flat_subimage, 4, 0, 1035),
	  GetAverage(flat_subimage, 4, 633, 520));

  fprintf(stderr, "final_image: corner = %lf, center = %lf\n",
	  GetAverage(final, 4, 0, 1035),
	  GetAverage(final, 4, 633, 520));

  return 0;
}


  

    

    
