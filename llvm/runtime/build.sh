# `llvm-config --cxxflags --ldflags --system-libs --libs core`

arch -arm64 clang -g  sds/sds.c wof_alloc/wof_allocator.c runtime.c String.c Object.c Integer.c PersistentList.c PersistentVector.c PersistentVectorNode.c -Wno-c++11-extensions -o runtime

