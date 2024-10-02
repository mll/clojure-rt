#ifndef RT_DEFINES
#define RT_DEFINES

#define REFCOUNT_TRACING
//#define REFCOUNT_NONATOMIC

#define TRUE 1
#define FALSE 0
typedef uint8_t BOOL;
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define HASHTABLE_THRESHOLD 8

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
   classType,
   deftypeType,
   concurrentHashMapType,
   keywordType,
   functionType,
   varType,
   bigIntegerType,
   ratioType,
   persistentArrayMapType,
};

typedef enum objectType objectType;

#endif
