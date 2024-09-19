#include <Image.h>
#include <stdio.h>

int main(int argc, char **argv) {
  const char *filename = argv[1];

  constexpr int x_ref = 107;
  constexpr int y_ref = 356;
  constexpr int span = 10;

  Image image(filename);
  
  fprintf(stderr, "        ");
  for (int x=x_ref-span; x<x_ref+span; x++) {
    fprintf(stderr, "%6d", x);
  }
  fprintf(stderr, "\n");
  
  for(int y=y_ref-span; y<y_ref+span; y++) {
    fprintf(stderr, "%d: ", y);
    for (int x=x_ref-span; x<x_ref+span; x++) {
      fprintf(stderr, "%6d", (int) (0.5 + image.pixel(x,y)));
    }
    fprintf(stderr, "\n");
  }
  return 0;
}

      
