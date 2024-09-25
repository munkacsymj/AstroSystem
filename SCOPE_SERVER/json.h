#include <string>
#include <list>

class JSON_Token;

class JSON_Expression {
 public:
  JSON_Expression(void);
  ~JSON_Expression(void);
  JSON_Expression(const char *byte_string);
  JSON_Expression Lookup(const char *lookup_seq);
  JSON_Expression Value(const char *keyword);
  JSON_Expression(std::list<JSON_Token *> &tokens);

  void Print(FILE *fp, int indent=0) const;

  const JSON_Expression *GetValue(const char *dot_string) const;

  double      Value_double(void) const;
  long        Value_int(void) const;
  const char *Value_char(void) const;
  bool        IsEmpty(void)  const { return j_type == JSON_EMPTY; }
  bool        IsSeq(void)    const { return j_type == JSON_SEQ; }
  bool        IsInt(void)    const { return j_type == JSON_INT; }
  bool        IsDouble(void) const { return j_type == JSON_FLOAT; }

 private:
  enum JASON_TYPE { JSON_SEQ, JSON_STRING, JSON_FLOAT, JSON_INT, JSON_ASSIGNMENT, JSON_EMPTY } j_type;
  double float_val;
  const char *string_val {nullptr};
  long int_val;
  const char *assignment_variable {nullptr};
  JSON_Expression *assignment_expression {nullptr};
  std::list<JSON_Expression *> seq_val;
};

