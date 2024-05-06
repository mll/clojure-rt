#ifndef RT_STATIC_STRING
#define RT_STATIC_STRING
#include "../codegen.h"

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
          std::pair<StaticCallType, StaticCall>>>>>> getStringStaticFunctions();

#endif
