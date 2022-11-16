#ifndef RT_FUNCTION
#define RT_FUNCTION
#include "String.h"
#include "defines.h"

#define INVOKATION_CACHE_SIZE 3

struct InvokationCache {
  uint64_t signature;
  unsigned char packed;
  unsigned char returnType;
  void *fptr;
};

typedef struct InvokationCache InvokationCache;

struct FunctionMethod  {
  unsigned char index;
  uint64_t fixedArity;
  uint64_t isVariadic;
  char *loopId;
  struct InvokationCache invokations[INVOKATION_CACHE_SIZE];
};

typedef struct FunctionMethod FunctionMethod;

struct Function {
  uint64_t uniqueId;
  uint64_t methodCount;
  uint64_t maxArity;
  struct FunctionMethod methods[];
};

typedef struct Function Function;

struct Function* Function_create(uint64_t methodCount, uint64_t uniqueId, uint64_t maxArity);

void Function_fillMethod(struct Function *self, uint64_t position, uint64_t index, uint64_t fixedArity,  BOOL isVariadic,  char *loopId);

BOOL Function_equals(struct Function *self, struct Function *other);
uint64_t Function_hash(struct Function *self);
String *Function_toString(struct Function *self); 
void Function_destroy(struct Function *self);

#endif
