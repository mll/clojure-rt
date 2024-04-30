#ifndef RT_STATIC_VAR
#define RT_STATIC_VAR
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
          std::pair<StaticCallType, StaticCall>>>>>> getVarStaticFunctions();

#endif
