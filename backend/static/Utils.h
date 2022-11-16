#ifndef RT_STATIC_UTILS
#define RT_STATIC_UTILS
#include "../codegen.h"

std::unordered_map<std::string, std::vector<std::pair<std::string, std::pair<StaticCallType, StaticCall>>>> getUtilsStaticFunctions();

#endif
