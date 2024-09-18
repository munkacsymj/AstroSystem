#include <stdio.h>
#include <Image.h>

double offset(int x, int y) {
  const double del_x = x-1006.5;
  const double del_y = y-281.26;
  return sqrt(del_x*del_x + del_y*del_y);
}

int main(int argc, char **argv) {
  Image image("/home/IMAGES/4-7-2021/u-aur_V.fits");

  const int center_x = 180;
  const int center_y = 389;
  
  for (int x=center_x-6; x<center_x+6; x++) {
    for (int y=center_y-6; y< center_y+6; y++) {
      const double v = image.pixel(x,y);
      printf("@(%d, %d): %.0lf, d=%.2lf\n", x, y, v, offset(x,y));
    }
  }

  return 0;
}
