#include <stdlib.h>
#include <stdio.h>

void usage(void) {
  fprintf(stderr, "usage: numexpand lll-hhh\n");
  exit(-2);
}

int main(int argc, char **argv) {
  int low, high;

  if(argc != 2) usage();

  char *arg = argv[1];

  if(arg[3] != '-') usage();

  arg[3] = 0;

  sscanf(arg, "%d", &low);
  sscanf(arg+4, "%d", &high);

  if(low > high ||
     low < 0 ||
     high < 0 ||
     (high - low) > 100) usage();

  while(low <= high) {
    printf("%03d ", low);
    low++;
  }
  return 0;
}
