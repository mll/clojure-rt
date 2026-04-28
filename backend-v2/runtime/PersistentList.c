#include "PersistentList.h"
#include "Object.h"
#include "RTValue.h"
#include <stdarg.h>

static PersistentList *EMPTY = NULL;
// How to mark
/* mem done */
void PersistentList_initialise() {
  if (EMPTY == NULL)
    EMPTY = PersistentList_create(RT_boxNull(), NULL);
}

PersistentList *PersistentList_empty() {
  Ptr_retain(EMPTY);
  return EMPTY;
}

void PersistentList_cleanup() {
  if (EMPTY) {
    Ptr_release(EMPTY);
    EMPTY = NULL;
  }
}

struct ListPair {
  PersistentList *first;
  PersistentList *last;
};

/* outside refcount system, mutable */
PersistentList *reverse(PersistentList *self) {
  if (self == NULL)
    return NULL;

  PersistentList *prev = NULL;
  PersistentList *current = self;
  PersistentList *next = NULL;

  while (current != NULL) {
    next = current->rest;
    current->rest = prev;
    prev = current;

    current = next;
  }

  PersistentList *temp = prev;
  uword_t runningCount = 0;

  while (temp != NULL) {
    temp->count = runningCount;
    runningCount++;
    temp = temp->rest;
  }

  return prev;
}

PersistentList *PersistentList_createMany(int32_t argCount, ...) {
  PersistentList *retVal = PersistentList_empty();

  va_list args;
  va_start(args, argCount);

  while (argCount > 0) {
    retVal = PersistentList_conj(retVal, va_arg(args, RTValue));
    argCount--;
  }

  va_end(args);
  return reverse(retVal);
}

/* outside refcount system */
PersistentList *PersistentList_create(RTValue first, PersistentList *rest) {
  PersistentList *self = allocate(sizeof(PersistentList));

  self->first = first;
  self->rest = rest;
  self->count = (rest ? rest->count : 0) + (RT_isNull(first) ? 0 : 1);

  Object_create((Object *)self, persistentListType);
  return self;
}

bool PersistentList_equals_managed(PersistentList *self, RTValue other) {
  return equals_managed(RT_boxPtr(self), other);
}

/* outside refcount system */
bool PersistentList_equals(PersistentList *self, PersistentList *other) {
  if (self->count != other->count)
    return false;

  PersistentList *selfPtr = self;
  PersistentList *otherPtr = other;

  while (selfPtr) {
    if (!(selfPtr->first == otherPtr->first ||
          (selfPtr->first && otherPtr->first &&
           equals(selfPtr->first, otherPtr->first))))
      return false;
    selfPtr = selfPtr->rest;
    otherPtr = otherPtr->rest;
  }
  return true;
}

/* outside refcount system */
uint64_t PersistentList_hash(PersistentList *self) {
  uint64_t h = 5381;
  h = combineHash(h, !RT_isNull(self->first) ? hash(self->first) : 5381);

  PersistentList *current = self->rest;
  while (current) {
    h = combineHash(h, PersistentList_hash(current));
    current = current->rest;
  }
  return h;
}

/* mem done */
String *PersistentList_toString(PersistentList *self) {
  String *retVal = String_create("(");
  String *space = String_create(" ");
  String *closing = String_create(")");

  PersistentList *current = self;

  while (current) {
    if (current != self && !RT_isNull(current->first)) {
      Ptr_retain(space);
      retVal = String_concat(retVal, space);
    }
    if (!RT_isNull(current->first)) {
      retain(current->first);
      String *s = toString(current->first);
      retVal = String_concat(retVal, s);
    }
    current = current->rest;
  }
  retVal = String_concat(retVal, closing);
  Ptr_release(space);
  Ptr_release(self);
  return retVal;
}

int32_t PersistentList_count(PersistentList *self) {
  int32_t retVal = self->count;
  Ptr_release(self);
  return retVal;
}

PersistentList *PersistentList_identity(PersistentList *self) { return self; }

RTValue PersistentList_next(PersistentList *self) {
  if (self->rest == NULL) {
    Ptr_release(self);
    return RT_boxNil();
  }

  PersistentList *next = self->rest;
  Ptr_retain(next);
  Ptr_release(self);
  return RT_boxPtr(next);
}

PersistentList *PersistentList_pop(PersistentList *self) {
  if (self->rest == NULL) {
    Ptr_release(self);
    return PersistentList_empty();
  }

  PersistentList *next = self->rest;
  Ptr_retain(next);
  Ptr_release(self);
  return next;
}

/* outside refcount system */
void PersistentList_destroy(PersistentList *self, bool deallocateChildren) {
  if (deallocateChildren) {
    PersistentList *child = self->rest;
    while (child) {
      PersistentList *next = child->rest;
      if (!Object_release_internal((Object *)child, false)) {
        break;
      }

      child = next;
    }
  }
  if (!RT_isNull(self->first))
    release(self->first);
}

void PersistentList_promoteToShared(PersistentList *self, uword_t current) {
  PersistentList *iter = self;
  while (iter != NULL && !(current & SHARED_BIT)) {
    promoteToShared(iter->first);
    Object_promoteToSharedShallow((Object *)iter, current);

    iter = iter->rest;
    if (iter != NULL) {
      current = Object_getRawRefCount((Object *)iter);
    }
  }
}

/* mem done */
PersistentList *PersistentList_conj(PersistentList *self, RTValue other) {
  return PersistentList_create(other, self);
}

RTValue PersistentList_first(PersistentList *self) {
  RTValue first = self->first;
  if (!RT_isNull(first)) {
    retain(first);
    Ptr_release(self);
    return first;
  }
  Ptr_release(self);
  return RT_boxNull();
}

PersistentList *PersistentList_fromArray(int32_t argCount, RTValue *args) {
  PersistentList *retVal = PersistentList_empty();
  for (int32_t i = argCount - 1; i >= 0; i--) {
    retVal = PersistentList_conj(retVal, args[i]);
  }
  return retVal;
}

RTValue RT_createListFromArray(int32_t argCount, RTValue *args) {
  return RT_boxPtr(PersistentList_fromArray(argCount, args));
}

RTValue PersistentList_reduce(PersistentList *self, RTValue f, RTValue start) {
  RTValue acc = start;
  PersistentList *current = self;

  FunctionMethod *method = Function_extractMethod(f, 2);

  size_t frameSize = sizeof(Frame) + 2 * sizeof(RTValue);
  Frame *frame = (Frame *)alloca(frameSize);
  frame->leafFrame = NULL;
  frame->bailoutEntryIndex = -1;

  RTValue args[2];

  while (current && !RT_isNull(current->first)) {
    PersistentList *next = current->rest;
    RTValue first = current->first;

    if (Ptr_isReusable(current)) {
      // Sole owner: steal children and destroy node immediately
      current->first = RT_boxNull();
      current->rest = NULL;
      Ptr_release(current);

      args[0] = acc;
      args[1] = first;
      // No retain(first) because we stole the reference from the destroyed node
      acc = RT_invokeMethodWithFrame(frame, f, method, args, 2);
    } else {
      // Shared: standard retain/release
      retain(first);
      if (next) Ptr_retain(next);

      args[0] = acc;
      args[1] = first;
      acc = RT_invokeMethodWithFrame(frame, f, method, args, 2);

      Ptr_release(current);
    }
    current = next;
  }

  if (current) Ptr_release(current);
  release(f);
  return acc;
}

RTValue PersistentList_reduce2(PersistentList *self, RTValue f) {
  if (RT_isNull(self->first)) {
    Ptr_release(self);
    return RT_invokeDynamic(f, NULL, 0);
  }

  RTValue first;
  PersistentList *rest;

  if (Ptr_isReusable(self)) {
    first = self->first;
    rest = self->rest;
    self->first = RT_boxNull();
    self->rest = NULL;
    Ptr_release(self);
  } else {
    first = self->first;
    retain(first);
    rest = self->rest;
    if (rest) Ptr_retain(rest);
    Ptr_release(self);
  }

  if (!rest) {
    release(f);
    return first;
  }

  return PersistentList_reduce(rest, f, first);
}
