arch -arm64 brew install llvm@16
brew unlink llvm@16 && brew link --force llvm@16
arch -arm64 brew install protobuf@21
brew unlink protobuf@21 && brew link --force protobuf@21
