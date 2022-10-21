#ifndef RT_NUMBERS
#define RT_NUMBERS
#include "codegen.h"

TypedValue Numbers_add(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args);
TypedValue Numbers_minus(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args);

map<string, StaticCall> getNumbersStaticFunctions();


#endif
