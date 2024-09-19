#include <Image.h>

int main(int argc, char **argv) {
  const char *filename = argv[1];
  printf("Rewriting %s\n", filename);
  Image i(filename);
  ImageInfo *ii = i.GetImageInfo();
  std::string d = ii->GetValueString("DEC_NOM");
  std::string r = ii->GetValueString("RA_NOM");
  ii->SetValueString("DEC_NOM", d);
  ii->SetValueString("RA_NOM", r);
  i.WriteFITSAuto(filename);
  return 0;
}
