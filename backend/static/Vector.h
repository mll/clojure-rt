#ifndef RT_STATIC_VECTOR
#define RT_STATIC_VECTOR
#include "../codegen.h"

std::pair<
  std::unordered_map<std::string,
    std::vector<
      std::pair<
        std::string,
        std::pair<
          StaticCallType,
          StaticCall>>>>,
  std::unordered_map<std::string,
    std::unordered_map<std::string,
      std::vector<
        std::pair<
          std::string,
          std::pair<
            StaticCallType,
            StaticCall>>>>>>
getVectorStaticAndInstanceFunctions();

#endif
