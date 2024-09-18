#include <stdio.h>
#include <scope_api.h>

int main(int argc, char **argv) {
  connect_to_scope();
  bool flipped = dec_axis_is_flipped();

  if (flipped) {
    printf("dec_axis_is_flipped() == true\n");
  } else {
    printf("dec_axis_is_flipped() == false\n");
  }
}
