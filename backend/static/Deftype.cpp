#include "Deftype.h"

extern "C" {
  typedef struct Deftype Deftype;
  Class *Deftype_getClass(Deftype *);
}

using namespace std;
using namespace llvm;

ObjectTypeSet GetClass_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return ObjectTypeSet(classType);
}

TypedValue GetClass(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("Deftype_getClass", ObjectTypeSet(classType), args);
}

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
            std::pair<StaticCallType, StaticCall>>>>>>
getDeftypeStaticFunctions() { // TODO: getClass should eventually work for any type
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> instanceCalls;
  unordered_map<string, vector<pair<string, void *>>> dynamicCalls;
  
  instanceCalls.insert({"getClass", {{"LT", {&GetClass_type, &GetClass}}}});
  dynamicCalls.insert({"getClass", {{"LT", (void *) &Deftype_getClass}}});
  
  return {{{(uint64_t) deftypeType, dynamicCalls}}, {{(uint64_t) deftypeType, instanceCalls}}};
}
