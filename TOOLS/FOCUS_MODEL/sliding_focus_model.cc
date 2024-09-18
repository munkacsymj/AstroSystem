#include <string.h>
#include <libgen.h>		// dirname(), basename()
#include <Image.h>
#include <stdio.h>
#include <unistd.h>		// getopt()
#include <list>
#include <assert.h>

int main(int argc, char **argv) {
  for (int f = 1; f < argc; f++) {
    char *orig_path1 = strdup(argv[f]);
    char *orig_path2 = strdup(argv[f]);

    char *image_name = basename(orig_path1);
    char *composite_dir = dirname(orig_path2);
    char *orig_image_name = (char *) malloc(strlen(image_name) +
					    strlen(composite_dir) +
					    10);
    sprintf(orig_image_name, "%s/../%s", composite_dir, image_name);
    Image *orig_image = new Image(orig_image_name);
    
    char worker_command[512];
    sprintf(worker_command, "/home/mark/ASTRO/CURRENT/TOOLS/FOCUS_MODEL/analyze_composite -i %s",
	    argv[f]);
    FILE *worker = popen(worker_command, "r");

    bool got_gaussian = false;
    bool got_smear = false;
    double gaussian;
    double smear;

    char buffer[132];
      
    while(fgets(buffer, sizeof(buffer), worker)) {
      double temp1;
      double temp2;
      double temp3;
      if(sscanf(buffer, "gaussian: %lf", &temp1) == 1) {
	gaussian = temp1;
	got_gaussian = true;
      } else if(sscanf(buffer, "Blur = %lf Smear = %lf Flux90 = %lf",
		       &temp1, &temp2, &temp3) == 3) {
	got_smear = true;
	smear = temp2;
      } else {
	fprintf(stderr, "unrecognized response: %s\n", buffer);
      }
    }
    pclose(worker);

    if (got_gaussian && got_smear) {
      const int num_stars = orig_image->GetIStarList()->NumStars;
      const JULIAN exp_time = orig_image->GetImageInfo()->GetExposureStartTime();
      const int focus = (int) (0.5 + orig_image->GetImageInfo()->GetFocus());

      if (num_stars > 6 && num_stars < 500) {
	printf("%.4lf, %d, %.3lf, %d, %.3lf, %s\n",
	       exp_time.day(), focus, gaussian, num_stars, smear, image_name);
      }
    }
    
    free(orig_path1);
    free(orig_path2);
    free(orig_image_name);
    delete orig_image;
  }
  return 0;
}
      
