#include <json.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>

struct ValueKeywordPair {
  const char *keyword;
  bool value_is_string;
  const char *string_val;
  int int_val;
};

class ImageProfile {
public:
  ImageProfile(const char *profile_name, const JSON_Expression *tree = nullptr);

  bool IsDefined(const char *keyword);
  int GetInt(const char *keyword);
  const char *GetChar(const char *keyword);
private:
  std::list<ValueKeywordPair *> keywords;
  bool profile_valid {false};
  ValueKeywordPair *FindByKeyword(const char *keyword);
};

int
ImageProfile::GetInt(const char *keyword) {
  ValueKeywordPair *pair = FindByKeyword(keyword);
  if (pair and not pair->value_is_string) return pair->int_val;
  fprintf(stderr, "ImageProfile::GetInt(%s): type mismatch.\n",
	  keyword);
  return -1; // Not a very good error return...
}

ValueKeywordPair *
ImageProfile::FindByKeyword(const char *keyword) {
  for (auto x : keywords) {
    if (strcmp(x->keyword, keyword) == 0) return x;
  }
  return nullptr;
}

ImageProfile::ImageProfile(const char *profile_name, const JSON_Expression *tree) {
  if (tree == nullptr) {
    const char *profile_filename = "/home/ASTRO/CURRENT_DATA/image_profiles.json";
    int fd = open(profile_filename, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "Unable to open %s\n", profile_filename);
      exit(-1);
    }

    struct stat statbuf;
    if (stat(profile_filename, &statbuf)) {
      fprintf(stderr, "Unable to stat() image_profiles from %s\n",
	      profile_filename);
    } else {
      char *profile_contents = (char *) malloc(statbuf.st_size+1);
      if (read(fd, profile_contents, statbuf.st_size) != statbuf.st_size) {
	fprintf(stderr, "Error reading image_profiles from %s\n",
		profile_filename);
	exit(-1);
      }
      close(fd);

      profile_contents[statbuf.st_size]=0;
      const JSON_Expression *profiles = new JSON_Expression(profile_contents);

      tree = profiles->GetValue("profiles");
      if (not tree->IsList()) {
	fprintf(stderr, "image_profiles.json: profiles are not in form of a list.\n");
	exit(-1);
      }
    }
  }

  // At this point, "tree" is an expression list (the assignment value
  // for "profiles").
  const JSON_Expression *match = nullptr;
  // Each "p" is a sequence.
  for (auto p : tree->Value_list()) {
    const JSON_Expression *name_expr = p->Value("name");
    if (name_expr->IsString() and
	strcmp(name_expr->Value_char(), profile_name) == 0) {
      match = p;
      break;
    }
  }
  if (match == nullptr) {
    fprintf(stderr, "ImageProfile: No profile found with name == %s\n",
	    profile_name);
    exit(-1);
  }
  // Check for an "include" using keyword "base".
  // "match" is a sequence with two or three assignments ("name",
  // "content", and "base")
  const JSON_Expression *base_expr = match->Value("base");
  if (base_expr) {
    ImageProfile base_profile(base_expr->Value_string().c_str(), tree);
    keywords = base_profile.keywords;
  }
  const JSON_Expression *content = match->Value("content");
  if (content and content->IsSeq()) {
    const std::list<std::string> flag_keywords { "offset",
					    "gain",
					    "mode",
					    "binning",
					    "compress",
					    "usb_traffic",
					    "format",
					    "box_bottom",
					    "box_height",
					    "box_left",
					    "box_width" };
    for (auto keyword : flag_keywords) {
      const JSON_Expression *this_value = content->Value(keyword.c_str());
      if (this_value) {
	ValueKeywordPair *this_pair = FindByKeyword(keyword.c_str());
	if (this_pair == nullptr) {
	  this_pair = new ValueKeywordPair;
	  keywords.push_back(this_pair);
	  this_pair->keyword = strdup(keyword.c_str());
	}
	this_pair->value_is_string = this_value->IsString();
	if (this_pair->value_is_string) {
	  this_pair->string_val = strdup(this_value->Value_string().c_str());
	} else {
	  this_pair->int_val = this_value->Value_int();
	}
      }
    }
  } else {
    fprintf(stderr, "Invalid or missing content in profie %s\n",
	    profile_name);
    exit(-1);
  }
  profile_valid = true;
}
  


int main(int argc, char **argv) {
  ImageProfile ip("finder");
  fprintf(stderr, "Selected profile gain = %d\n",
	  ip.GetInt("gain"));
  return 0;
}
  
