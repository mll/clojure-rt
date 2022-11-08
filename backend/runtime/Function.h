#ifndef RT_FUNCTION
#define RT_FUNCTION
#include "String.h"
#include "defines.h"


struct FunctionMethod  {
  uint64_t fixedArity;
  uint64_t isVariadic;
  char *loopId;
};

typedef struct FunctionMethod FunctionMethod;

struct Function {
  char *uniqueName;
  uint64_t methodCount;
  uint64_t hash;
  uint64_t maxArity;
  FunctionMethod methods[];
};

typedef struct Function Function;

Function* Function_create(uint64_t methodCount, char *uniqueName, uint64_t hash, uint64_t maxArity);


void Function_fillMethod(Function *self, uint64_t position, uint64_t fixedArity,  BOOL isVariadic,  char *loopId);

BOOL Function_equals(Function *self, Function *other);
uint64_t Function_hash(Function *self);
String *Function_toString(Function *self); 
void Function_destroy(Function *self);

#endif