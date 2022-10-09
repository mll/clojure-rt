arch -arm64 clang++ -O3 runtime.cpp String.cpp Object.cpp Number.cpp  `llvm-config --cxxflags --ldflags --system-libs --libs core` -Wno-c++11-extensions -o runtime
# -O3 -g
