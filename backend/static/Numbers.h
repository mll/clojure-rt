#ifndef RT_STATIC_NUMBERS
#define RT_STATIC_NUMBERS
#include "../codegen.h"

std::unordered_map<std::string, std::vector<std::pair<std::string, std::pair<StaticCallType, StaticCall>>>> getNumbersStaticFunctions();


#endif
