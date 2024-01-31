#ifndef RT_STATIC_VECTOR
#define RT_STATIC_VECTOR
#include "../codegen.h"

std::unordered_map<std::string, std::vector<std::pair<std::string, std::pair<StaticCallType, StaticCall>>>> getVectorStaticFunctions();


#endif
