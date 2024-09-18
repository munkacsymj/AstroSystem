#include <Image.h>
#include <unistd.h>		// getopt()
#include <stdlib.h>		// exit()
#include <stdio.h>

void usage(void) {
  fprintf(stderr, "usage: fixup -i in_file.fits -o out_file.fits\n");
  exit(-2);
  /*NOTREACHED*/
}

int main(int argc, char **argv) {
  int ch;
  const char *in_file = nullptr;
  const char *out_file = nullptr;

  while((ch = getopt(argc, argv, "i:o:")) != -1) {
    switch(ch) {
    case 'i':
      in_file = optarg;
      break;

    case 'o':
      out_file = optarg;
      break;

    case '?':
    default:
      usage();
      exit(-2);
    }
  }

  if (in_file == nullptr or out_file == nullptr) usage();

  Image i(in_file);

  for (int y=0; y<i.height; y++) {
    for (int x=0; x<i.width; x++) {
      const double v = i.pixel(x,y);
      if (v < 0.0) i.pixel(x,y) = 0.0;
    }
  }

  i.WriteFITS32(out_file, false /*no compression*/);

  return 0;
}
