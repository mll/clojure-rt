#include "../Exceptions.h"
#include <stdio.h>
#include <stdlib.h>

void throwLanguageException_C(const char *name, RTValue message,
                              RTValue payload) {
  fprintf(stderr, "RT Exception Thrown: %s\n", name);
  abort();
}

void throwArityException_C(int expected, int actual) {
  fprintf(stderr, "ArityException: expected %d, got %d\n", expected, actual);
  abort();
}

void throwIllegalArgumentException_C(const char *message) {
  fprintf(stderr, "IllegalArgumentException: %s\n", message);
  abort();
}

void throwIllegalStateException_C(const char *message) {
  fprintf(stderr, "IllegalStateException: %s\n", message);
  abort();
}

void throwUnsupportedOperationException_C(const char *message) {
  fprintf(stderr, "UnsupportedOperationException: %s\n", message);
  abort();
}

void throwArithmeticException_C(const char *message) {
  fprintf(stderr, "ArithmeticException: %s\n", message);
  abort();
}

void throwInternalInconsistencyException_C(const char *errorMessage) {
  fprintf(stderr, "InternalInconsistencyException: %s\n", errorMessage);
  abort();
}
