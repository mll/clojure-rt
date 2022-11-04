#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

//Declaring global variables to
//be shared betweeen threads
 
typedef struct AtomTest {
  atomic_ullong atomic_count;	// atomic variable
  int64_t non_atomic_count;    // non-atomic variable
} AtomTest;

size_t chunkSize = 0; 

void *func(void* input)
{
    uint8_t *memory = input; 
    for(int n = 0; n < 1000; ++n) {
      for (int j = 0; j < 100000; j++) {

        AtomTest *aa = (AtomTest *)(memory + chunkSize * j);
        atomic_fetch_add(&(aa->atomic_count), 1); // atomic updation
        ++aa->non_atomic_count; // non-atomic updation
      }
      
    }
    
    return 0;
}
 
int main(void)
{
    chunkSize = MAX(sizeof(AtomTest), 300);
    uint8_t *memory = malloc(100000 * chunkSize); 
    AtomTest *aa = (AtomTest *)memory;
    atomic_store_explicit (&(aa->atomic_count), 0, memory_order_relaxed);
    int a = atomic_load_explicit(&(aa->atomic_count), memory_order_relaxed);	
    printf("%d", a);
    /* for(int i=0; i<100000; i++) { */
    /*   AtomTest *aa = (AtomTest *)(memory + chunkSize * i); */
    /*   atomic_store_explicit (&(aa->atomic_count), 0, memory_order_relaxed); */
    /*   aa->non_atomic_count = 0; */
    /*   aa->atomic_count = 0; */
    /* } */

    /* /\* pthread_t threads[10]; *\/ */
    /* /\* for(int i = 0; i < 10; i++) *\/ */
    /* /\*     pthread_create(&threads[i], NULL, func, memory); *\/ */
        
    /* /\* for(int i = 0; i < 10; i++) *\/ */
    /* /\*     pthread_join(threads[i], NULL); *\/ */
 
    /* for(int i=0; i<100000; i++) { */
    /*   AtomTest *aa = (AtomTest *)(memory + chunkSize * i); */
    /*   printf("The atomic counter is: %llu\n", aa->atomic_count); */
    /*   printf("The non-atomic counter is: %llu\n", aa->non_atomic_count); */
    /* } */
}
