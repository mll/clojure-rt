#include "TestTools.h"

jmp_buf exception_env;
char *last_exception_name = NULL;
bool exception_catchable = false;

void reset_exception_state() {
  last_exception_name = NULL;
  exception_catchable = false;
}

static void handle_exception(const char *name) {
  last_exception_name = (char *)name;
  if (exception_catchable) {
    longjmp(exception_env, 1);
  }
}

void throwLanguageException_C(const char *name, RTValue message,
                              RTValue payload) {
  handle_exception(name);
  fprintf(stderr, "RT Exception Thrown: %s\n", name);
  abort();
}

void throwArityException_C(int expected, int actual) {
  handle_exception("ArityException");
  fprintf(stderr, "ArityException: expected %d, got %d\n", expected, actual);
  abort();
}

void throwIllegalArgumentException_C(const char *message) {
  handle_exception("IllegalArgumentException");
  fprintf(stderr, "IllegalArgumentException: %s\n", message);
  abort();
}

void throwIllegalStateException_C(const char *message) {
  handle_exception("IllegalStateException");
  fprintf(stderr, "IllegalStateException: %s\n", message);
  abort();
}

void throwUnsupportedOperationException_C(const char *message) {
  handle_exception("UnsupportedOperationException");
  fprintf(stderr, "UnsupportedOperationException: %s\n", message);
  abort();
}

void throwArithmeticException_C(const char *message) {
  handle_exception("ArithmeticException");
  fprintf(stderr, "ArithmeticException: %s\n", message);
  abort();
}

void throwIndexOutOfBoundsException_C(uword_t index, uword_t count) {
  handle_exception("IndexOutOfBoundsException");
  fprintf(stderr, "IndexOutOfBoundsException: index %lu, count %lu\n", index,
          count);
  abort();
}

void throwInternalInconsistencyException_C(const char *errorMessage) {
  handle_exception("InternalInconsistencyException");
  fprintf(stderr, "InternalInconsistencyException: %s\n", errorMessage);
  abort();
}
 
void JITEngine_slowPath_enter(void *engine, void *epochPtr) {
   // No-op for tests
}
 
void JITEngine_slowPath_leave(void *engine) {
   // No-op for tests
}
 
Class *ClassLookup(const char *className, void *jitEngine) {
   fprintf(stderr, "Mock ClassLookup called for: %s\n", className);
   abort();
}
