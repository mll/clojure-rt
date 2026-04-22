We're building a clojure JIT compiler in LLVM/C++ API. We use reference counting with consuming semantics and NaN boxing.

There's two components here - one is the frontend - a clojure program that creates AST, computes memory guidance (dropMemory for happy path and unwindMemory for landing pads)
and builds the binary proto.

The second - the backend which reads proto and compiles/executes.

Our JIT currently compiles the proto forms using LLVMs ORC2 and links them, on IR level with runtime written in C - which allows for huge inlining optimisations.

When working with tests - always stop and ask for permission if you want to change non-test code.