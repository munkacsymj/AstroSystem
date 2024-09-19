#include <stdio.h>
#include <Image.h>

int main(int argc, char **argv) {
  Image image("/tmp/imageq.fits");

  for (int x = 508; x<513; x++) {
    for (int y=508; y<513; y++) {
      double v = image.pixel(x,y);
      fprintf(stderr, "(%d, %d): %lf\n", x, y, v);
    }
  }
  return 0;
}
