#ifndef RT_DEFINES
#define RT_DEFINES

#define REFCOUNT_TRACING
//#define REFCOUNT_NONATOMIC
//#define USE_MEMORY_BANKS

#define TRUE 1
#define FALSE 0
typedef uint8_t BOOL;
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define HASHTABLE_THRESHOLD 8


#endif
