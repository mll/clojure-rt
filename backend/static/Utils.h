#ifndef RT_STATIC_UTILS
#define RT_STATIC_UTILS
#include "../codegen.h"

unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> getUtilsStaticFunctions();

#endif
