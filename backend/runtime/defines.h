#ifndef RT_DEFINES
#define RT_DEFINES

#define TRUE 1
#define FALSE 0
typedef uint8_t BOOL;

enum objectType {
   runtimeDeterminedType,
   integerType,
   stringType,
   persistentListType,
   persistentVectorType,
   persistentVectorNodeType,
   doubleType,
   nilType,
   booleanType
};


#endif
