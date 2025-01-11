#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>

enum objectType {
   integerType = 1,
   stringType,
   persistentListType,
   persistentVectorType,
   persistentVectorNodeType,
   doubleType,
   nilType,
   booleanType,
   symbolType,
   concurrentHashMapType,
   keywordType,
   functionType,
   bigIntegerType,
   ratioType,
   peristsentArrayMap,
};

typedef enum objectType objectType;

struct Object {
  objectType type;
  volatile atomic_uint_fast64_t atomicRefCount;
};




main() {
  struct Object *a = malloc(sizeof(struct Object));

  a->type = functionType;
  a->atomicRefCount = 2;
  printf("SIZE: %lu", sizeof(struct Object));
  printf("SIZE: %d", a->type);

}
