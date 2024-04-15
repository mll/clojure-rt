#ifndef RT_STATIC_VECTOR
#define RT_STATIC_VECTOR
#include "../codegen.h"

std::pair<
  std::unordered_map<
    std::string, // static method name
    std::vector<
      std::pair<
        std::string, // signature
        std::pair<StaticCallType, StaticCall>>>>,
  std::pair<
    std::unordered_map<uint64_t, // classId
      std::unordered_map<std::string, // methodName
        std::vector<
          std::pair<
            std::string, // signature
            void *>>>>,
    std::unordered_map<uint64_t, // classId
      std::unordered_map<std::string, // methodName
        std::vector<
          std::pair<
            std::string, // signature
            std::pair<StaticCallType, StaticCall>>>>>>>
getVectorFunctions();

#endif
