arch -arm64 clang++ -g -O3 kaleydoscope.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native` -o kaleydoscope
