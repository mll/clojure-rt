#ifndef RT_DEFINES
#define RT_DEFINES

#define TRUE 1
#define FALSE 0
typedef uint8_t BOOL;
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

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
};

typedef enum objectType objectType;

#endif
