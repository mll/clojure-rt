#include "Object.h"
#include "Class.h"
#include "Hash.h"
#include "word.h"
#include <stdarg.h>


Class *Class_createInterface(String *name, String *className,
                             int32_t extendsInterfaceCount,
                             Class **extendsInterfaces,
                             int32_t staticMethodCount,
                             RTValue *staticMethodNames,
                             ClojureFunction **staticMethods,
                             int32_t methodCount, RTValue *methodNames) {
  Class *retVal = Class_create(
      name, className, extendsInterfaceCount, extendsInterfaces, 0, NULL, NULL,
      staticMethodCount, staticMethodNames, staticMethods, 0, NULL, 
      methodCount, methodNames, NULL, 0, NULL);
  retVal->isInterface = true;
  return retVal;
}  
    
Class *Class_create(String *name, String *className,
                    int32_t superclassCount, Class **superclasses,

                    int32_t staticFieldCount, RTValue *staticFieldNames,
                    RTValue *staticFields,

                    int32_t staticMethodCount, RTValue *staticMethodNames,
                    ClojureFunction **staticMethods,

                    int32_t fieldCount, RTValue *fieldNames,
                    int32_t methodCount, RTValue *methodNames,
                    ClojureFunction **methods,

                    int32_t implementedInterfacesCount,
                    ImplementedInterface **implementedInterfaces) {
  
  Class *self = allocate(sizeof(Class));
  self->isInterface = false;
  self->registerId = 0; // unregistered
  self->name = name;
  self->className = className;
  self->superclassCount = superclassCount;
  self->superclasses = superclasses;
  
  self->staticFieldCount = staticFieldCount;
  self->staticFieldNames = staticFieldNames;
  self->staticFields = staticFields;
  
  self->staticMethodCount = staticMethodCount;
  self->staticMethodNames = staticMethodNames;
  self->staticMethods = staticMethods;
  
  self->methodCount = methodCount;
  self->methodNames = methodNames;
  self->methods = methods;
  
  self->implementedInterfacesCount = implementedInterfacesCount;
  self->implementedInterfaces = implementedInterfaces;
  self->superclasses = superclasses;
  Object_create((Object *)self, classType);
  return self;
}

/* outside refcount system */
bool Class_equals(Class *self, Class *other) {
  // Unregistered classes should never appear here
  return self->registerId == other->registerId;
}

/* outside refcount system */
int32_t Class_hash(Class *self) { // CONSIDER: Ignoring fields for now, is it wise?
  return combineHash(avalanche(self->registerId), Object_hash((Object *)self->name));
}

String *Class_toString(Class *self) {
  String *retVal = self->className;
  Ptr_retain(retVal);
  Ptr_release(self);
  return retVal;
}

void Class_destroy(Class *self) {
  Ptr_release(self->name);
  Ptr_release(self->className);
  for (int32_t i = 0; i < self->superclassCount; i++)
    Ptr_release(self->superclasses[i]);

  if(self->superclasses) deallocate(self->superclasses);  
  for (int32_t i = 0; i < self->staticFieldCount; ++i) {
    release(self->staticFieldNames[i]);
    release(self->staticFields[i]);
  }
  if (self->staticFieldNames) deallocate(self->staticFieldNames);
  if (self->staticFields) deallocate(self->staticFields);
  
  for (int32_t i = 0; i < self->staticMethodCount; ++i) {
    release(self->staticMethodNames[i]);
    Ptr_release(self->staticMethods[i]);
  }
  if (self->staticMethodNames) deallocate(self->staticMethodNames);
  if (self->staticMethods) deallocate(self->staticMethods);
  
  for (int32_t i = 0; i < self->fieldCount; ++i) {
    release(self->fieldNames[i]);
  }
  
  if (self->fieldNames) deallocate(self->fieldNames);
  
  for (int32_t i = 0; i < self->methodCount; ++i) {
    release(self->methodNames[i]);
    // Will be NULL for interfaces
    if(self->methods) Ptr_release(self->methods[i]);
  }
  if (self->methodNames) deallocate(self->methodNames);
  if (self->methods) deallocate(self->methods);
  
  for (int32_t i = 0; i < self->implementedInterfacesCount; i++) {
    for (int32_t j = 0; j < self->implementedInterfaces[i]->interface->methodCount; j++) {
      Ptr_release(self->implementedInterfaces[i]->functions[j]);
    }
    Ptr_release(self->implementedInterfaces[i]->interface);
    if (self->implementedInterfaces[i]->functions)
      deallocate(self->implementedInterfaces[i]->functions);
    deallocate(self->implementedInterfaces[i]);
  }
  if (self->implementedInterfaces) deallocate(self->implementedInterfaces);
}

int32_t Class_fieldIndex(Class *self, RTValue field) {
  int32_t retVal = -1;
  for (int32_t i = 0; i < self->fieldCount; i++) {
    RTValue fieldName = self->fieldNames[i];
    if (equals(fieldName, field)) {
      retVal = i;
    }      
  }      
  Ptr_release(self);
  release(field);  
  return retVal;
}

int32_t Class_staticFieldIndex(Class *self, RTValue staticField) {
  int32_t retVal = -1;
  for (int32_t i = 0; i < self->staticFieldCount; i++) {
    RTValue fieldName = self->staticFieldNames[i];
    if (equals(fieldName, staticField)) {
      retVal = i;
    }      
  }      
  Ptr_release(self);
  release(staticField);
  return retVal;
}

RTValue Class_setIndexedStaticField(Class *self, int32_t i, RTValue value) {
  assert (i >= 0 && i < (int32_t)self->staticFieldCount && "Internal error - unsafe index");

  RTValue oldValue = self->staticFields[i];
  self->staticFields[i] = value;
  release(oldValue);
  Ptr_release(self);
  return value;
}

RTValue Class_getIndexedStaticField(Class *self, int32_t i) {
  assert (i >= 0 && i < (int32_t)self->staticFieldCount && "Internal error - unsafe index");  
  RTValue retVal = self->staticFields[i];
  retain(retVal);
  Ptr_release(self);
  return retVal;
}

ClojureFunction *Class_resolveInstanceCall(Class *self, RTValue name, int32_t argCount) {
  // Class method
  ClojureFunction *retVal = NULL;
  for (int32_t i = 0; i < self->methodCount; i++) {
    if (equals(name, self->methodNames[i])
        && Function_validCallWithArgCount(self->methods[i], argCount)) {
      retVal = self->methods[i];
      Ptr_retain(retVal);
      Ptr_release(self);
      release(name);
      return retVal;
    }
  }
  
  // Interface implementation
  for (int32_t i = 0; i < self->implementedInterfacesCount; i++) {
    for (int32_t j = 0; j < self->implementedInterfaces[i]->interface->methodCount; j++) {
      if (equals(name, self->implementedInterfaces[i]->interface->methodNames[j])
          && Function_validCallWithArgCount(self->implementedInterfaces[i]->functions[j], argCount)) {
        retVal = self->implementedInterfaces[i]->functions[j];
        Ptr_retain(retVal);
        Ptr_release(self);
        release(name);
        return retVal;
      }
    }
  }

  // Resolve in superclasses  
  for (int32_t i = 0; i < self->superclassCount; i++) {
    Ptr_retain(self->superclasses[0]);
    retVal = Class_resolveInstanceCall(self->superclasses[i], name, argCount);
    if (retVal) {
      Ptr_release(self);
      return retVal;
    }      
  }    
  

  Ptr_release(self);
  release(name);
  
  /* NULL signals the function was not found. */
  return retVal;
}
