#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "aavso_photometry_int.h"

const char *mystringdup(const char *start, const char *end);

int main(int argc, char **argv) {
  char buffer[4096*64];
  int fd = open("./sample.json", O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Error opening sample data.\n");
  }
  int n = read(fd, buffer, sizeof(buffer));
  fprintf(stderr, "Read %d bytes from sample data file.\n", n);
  buffer[n]=0;
  
  const char *end;
  PObject *p = ParseObject(buffer, &end);
  fprintf(stderr, "Done parsing.\n");

  PRecord *r = p->AsRecord();
  PObject *d = r->GetValue("chartid");
  fprintf(stderr, "ChartID = %s\n", d->AsString());
  PList *ph = r->GetValue("photometry")->AsList();
  for (auto item : ph->items) {
    PRecord *check_star = item->AsRecord();
    fprintf(stderr, "%s\n", check_star->GetValue("auid")->AsString());
  }
}
