#include <Image.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void usage(void) {
  fprintf(stderr, "usage: fix_gradient -i image.fits\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int ch;
  const char *image_name = nullptr;

  // Command line options:
  // -i file.fits

  while((ch = getopt(argc, argv, "i:")) != -1) {
    switch(ch) {
    case 'i':
      image_name = optarg;
      break;

    case '?':
    default:
      usage();
    }
  }

  if (image_name == nullptr) usage();

  Image image(image_name);

  ImageInfo *info = image.GetImageInfo();
  if (!info) {
    fprintf(stderr, "Error: Image has no EXPOSURE keyword.\n");
    exit(-2);
  } else {
    double exposure = -1.0;

    if (info->ExposureDurationValid()) {
      exposure = info->GetExposureDuration();
    }

    if (exposure > 0.0) {
      image.RemoveShutterGradient(exposure);
      image.WriteFITSFloat(image_name);
    } else {
      fprintf(stderr, "Error: EXPOSURE keyword missing or invalid value.\n");
      exit(-2);
    }
  }

  return 0;
}
