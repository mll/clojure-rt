We're building a clojure JIT compiler in LLVM/C++ API. 

There's two components here - one is the frontend - a clojure program that creates AST, computes memory guidance (dropMemory for happy path and unwindMemory for landing pads) and builds the binary proto.


In the backend we use reference counting with consuming semantics and NaN boxing.

The entire memory architecture relies on consuming semantics. The only exceptions are runtime functions explicitly annotated with 'outside ref count', which operate on borrowing semantics. There is also a special case regarding the reference cycle between a namespace and a var—since a var is designed to hold a weak reference to its namespace, an additional release is required during its construction to properly satisfy the consuming semantics.

The second - the backend which reads proto and compiles/executes.

Our JIT currently compiles the proto forms using LLVMs ORC2 and links them, on IR level with runtime written in C - which allows for huge inlining optimisations.

When working with tests - always stop and ask for permission if you want to change non-test code.

