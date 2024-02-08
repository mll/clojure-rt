A manual for adding an additional basic type to the runtime:

I. Create NewType.h and NewType.c files in backend/runtime (You can base on Boolean.h/c) and add .c file in runtime/CMakeLists.txt
II. The new type has to have the following functions witten in their h/c files:
   a) NewType_equals
   b) NewType_hash
   c) NewType_toString
   d) NewType_destroy

   e) Possible constructors: NewType_create
   f) Whatever other runtime functions are needed.

  All of them need to be implemented and declared in the header. 
  The header has to declare our new type. No obligatory fields are defined here, 
  the Object struct is "glued" just before our type so that memory layout is this:

  1. Object (look into Object.h) fields
  2. Memory alignment offset (sometimes 0)
  3. Fields typical of our NewType

III. Add the desired types enum to defines.h (add it before last type, which should remain persistentArrayMapType)

IV. Edit Object.h and add appropriate clauses to 
   a) Object_destroy 
   b) Object_hash
   c) Object_equals
   d) Object_toString

   Please note that any 'create' functions you added in II. are to be used directly in the compiler, 
   not through Object.h (see V.b.1)

V. Go to compiler and modify
   a) ObjectTypeSet.h
      1. Add the apropriate constant class for compiler level constant tracking (not every type has it).
      2. Add your type to 'static ObjectTypeSet all()'
      3. Invent a two-letter signature (starting with "L" if it is packed, without "L" if native)
         and add it to typeStringForArg
   b) codegen.cpp
      1. typeForArgString
   c) RuntimeInterop.cpp
      1. Add your type to dynamicCreate. Then, the type can be created directly from the nodes using 
         this function.
      2. You may need to introspect your type in LLVM generated code. If so, add the appropriate 
         'runtimeNewTypeType' that creates LLVM type that should be 100% compatible with what you did in         I.

VI. Add your new type to the compiler in node generators (ops/...). The usage will depend on the purpose of your new type. Make warnings should guide you when in doubt.
