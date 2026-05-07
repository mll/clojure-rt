#ifndef RT_DEFINES
#define RT_DEFINES

#define REFCOUNT_TRACING
//#define REFCOUNT_NONATOMIC
//#define USE_MEMORY_BANKS

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
// #define HASHTABLE_THRESHOLD 8

#define HASHTABLE_THRESHOLD 128

#define EBR_FLUSH_NODE_THRESHOLD 20

#endif
