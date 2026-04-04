#include "TestTools.h"
#include "runtime/Object.h"
#include "runtime/String.h"

jmp_buf exception_env;
char *last_exception_name = NULL;
bool exception_catchable = false;

__attribute__((weak)) void reset_exception_state() {
  last_exception_name = NULL;
  exception_catchable = false;
}

static void handle_exception(const char *name) {
  last_exception_name = (char *)name;
  if (exception_catchable) {
    longjmp(exception_env, 1);
  }
}

__attribute__((weak)) void
throwLanguageException_C(const char *name, RTValue message, RTValue payload) {
  handle_exception(name);
  fprintf(stderr, "RT Exception Thrown: %s\n", name);
  abort();
}

__attribute__((weak)) void throwArityException_C(int expected, int actual) {
  handle_exception("ArityException");
  fprintf(stderr, "ArityException: expected %d, got %d\n", expected, actual);
  abort();
}

__attribute__((weak)) void
throwIllegalArgumentException_C(const char *message) {
  handle_exception("IllegalArgumentException");
  fprintf(stderr, "IllegalArgumentException: %s\n", message);
  abort();
}

__attribute__((weak)) void throwIllegalStateException_C(const char *message) {
  handle_exception("IllegalStateException");
  fprintf(stderr, "IllegalStateException: %s\n", message);
  abort();
}

__attribute__((weak)) void
throwUnsupportedOperationException_C(const char *message) {
  handle_exception("UnsupportedOperationException");
  fprintf(stderr, "UnsupportedOperationException: %s\n", message);
  abort();
}

__attribute__((weak)) void throwArithmeticException_C(const char *message) {
  handle_exception("ArithmeticException");
  fprintf(stderr, "ArithmeticException: %s\n", message);
  abort();
}

__attribute__((weak)) void throwIndexOutOfBoundsException_C(uword_t index,
                                                            uword_t count) {
  handle_exception("IndexOutOfBoundsException");
  fprintf(stderr, "IndexOutOfBoundsException: index %lu, count %lu\n", index,
          count);
  abort();
}

__attribute__((weak)) void throwInternalInconsistencyException_C(String *msg) {
  handle_exception("InternalInconsistencyException");
  String *s = String_compactify(msg);
  fprintf(stderr, "InternalInconsistencyException: %s\n", String_c_str(s));
  Ptr_release(s);
  abort();
}

__attribute__((weak)) void JITEngine_slowPath_enter(void *engine,
                                                    void *epochPtr) {
  // No-op for tests
}

__attribute__((weak)) void JITEngine_slowPath_leave(void *engine) {
  // No-op for tests
}

__attribute__((weak)) Class *ClassLookupByName(const char *className,
                                               void *jitEngine) {
  fprintf(stderr, "Mock ClassLookup called for: %s\n", className);
  abort();
}

__attribute__((weak)) Class *ClassLookupByRegisterId(int32_t registerId,
                                                     void *jitEngine) {
  fprintf(stderr, "Mock ClassLookup called for: %d\n", registerId);
  abort();
}

__attribute__((weak)) String *exceptionToString_C(void *exception) {
  return String_createStatic("Exception");
}

__attribute__((weak)) void *
createException_C(const char *className, String *message, RTValue payload) {
  Ptr_release(message);
  release(payload);
  return (void *)0x1;
}

__attribute__((weak)) void deleteException_C(void *exception) {}