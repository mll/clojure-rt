#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Define RT_NO_STRING_H to prevent RTValue.h from including <string.h> (and
// finding the local String.h)
#define RT_NO_STRING_H
#include "../../RuntimeHeaders.h"

// Correct declaration for delete_class_description which is in EdnParser.cpp
void delete_class_description(void *ptr);

void test_initialise_memory() { initialise_memory(); }

void test_RuntimeInterface_cleanup() { RuntimeInterface_cleanup(); }

String *test_String_create(const char *str) { return String_create(str); }

Class *test_Class_create(String *name, String *simpleName, int flags,
                         void *ext) {
  return Class_create(name, simpleName, flags, ext);
}

void test_delete_class_description(void *p) { delete_class_description(p); }

void test_release(RTValue v) { release(v); }
