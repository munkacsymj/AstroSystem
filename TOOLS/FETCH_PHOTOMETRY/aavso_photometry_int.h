#include <list>
#include <stdlib.h>

struct FieldPair {
  const char *fieldname;
  const char *fieldvalue;
};

class PPair;
class PObject;
class PRecord;
class PList;
typedef std::list<PPair *> PairList;
typedef std::list<PObject *> PObjectList;

enum PObjectType { PTRecord, PTList, PTPair, PTBoolean, PTNumber, PTString };

class PObject {
public:
  PObject(PObjectType t) { p_type = t; }
  virtual ~PObject(void) {;}
  PObjectType p_type;

  bool ObjectIsRecord(void);
  bool ObjectIsList(void);
  bool ObjectIsPair(void);
  bool ObjectIsString(void);
  PRecord *AsRecord(void);
  PList *AsList(void);
  PPair *AsPair(void);
  const char *AsString(void);
};

class PRecord : public PObject {
public:
  PRecord(void) : PObject(PTRecord) {;}
  ~PRecord(void) {;}
  PairList pairs;
  PObject *GetValue(const char *fieldname);
};

class PList : public PObject {
public:
  PList(void) : PObject(PTList) {;}
  ~PList(void) {;}
  PObjectList items;
};

class PPair : public PObject {
public:
  PPair(void) : PObject(PTPair) {;}
  ~PPair(void) { free((void *)fieldname); delete value_object; }
  const char *fieldname;
  PObject *value_object;
};

class PBoolean : public PObject {
public:
  PBoolean(void) : PObject(PTBoolean) {;}
  ~PBoolean(void) {;}
  bool bool_value;
};

class PNumber : public PObject {
public:
  PNumber(void) : PObject(PTNumber) {;}
  ~PNumber(void) {;}
  double number_value;
};

class PString : public PObject {
public:
  PString(void) : PObject(PTString) {;}
  ~PString(void) { free((void *)string_value); }
  const char *string_value;
};

PObject *ParseObject(const char *buffer, const char **next);
PString *ParseString(const char *buffer, const char **next);
PPair *ParsePair(const char *buffer, const char **next);
PRecord *ParseRecord(const char *buffer, const char **next);
PList *ParseList(const char *buffer, const char **next);
PBoolean *ParseBoolean(const char *buffer, const char **next);
PNumber *ParseNumber(const char *buffer, const char **next);

