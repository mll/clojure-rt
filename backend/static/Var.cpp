#include "Var.h"  
#include <math.h>

using namespace std;
using namespace llvm;

extern "C" {
  typedef struct Var Var;
  typedef struct Nil Nil;
  BOOL *Var_hasRoot(Var *self);
  void *Var_deref(Var *self);
  Nil *Var_bindRoot(Var *self, Object *object);
  Nil *Var_unbindRoot(Var *self);
}

ObjectTypeSet HasRoot_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return ObjectTypeSet(booleanType);
}

TypedValue HasRoot(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("Var_hasRoot", ObjectTypeSet(booleanType), args);
}

// Many other things can be derefed, but for now it is only var
ObjectTypeSet Deref_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return ObjectTypeSet::all();
}

TypedValue Deref(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("Var_deref", ObjectTypeSet::all(), args);
}

ObjectTypeSet BindRoot_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return ObjectTypeSet(nilType);
}

TypedValue BindRoot(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("Var_bindRoot", ObjectTypeSet(nilType), args);
}

ObjectTypeSet UnbindRoot_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return ObjectTypeSet(nilType);
}

TypedValue UnbindRoot(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("Var_unbindRoot", ObjectTypeSet(nilType), args);
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
getVarStaticFunctions() {
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> instanceCalls;
  vector<pair<string, pair<StaticCallType, StaticCall>>> hasRoot, deref, bindRoot, unbindRoot;
  
  unordered_map<string, vector<pair<string, void *>>> dynamicCalls;
  vector<pair<string, void *>> hasRootDynamic, derefDynamic, bindRootDynamic, unbindRootDynamic;

  hasRoot.push_back({"LQ", {&HasRoot_type, &HasRoot}});
  instanceCalls.insert({"hasRoot", hasRoot});
  hasRootDynamic.push_back({"LQ", (void *) &Var_hasRoot});
  dynamicCalls.insert({"hasRoot", hasRootDynamic});

  deref.push_back({"LQ", {&HasRoot_type, &HasRoot}});
  instanceCalls.insert({"deref", deref});
  derefDynamic.push_back({"LQ", (void *) &Var_hasRoot});
  dynamicCalls.insert({"deref", derefDynamic});

  auto types = ObjectTypeSet::allTypes();
  for (auto t: types) bindRoot.push_back({"LQ" + t, {&BindRoot_type, &BindRoot}});
  instanceCalls.insert({"bindRoot", bindRoot});
  for (auto t: types) bindRootDynamic.push_back({"LQ" + t, (void *) &Var_bindRoot});
  dynamicCalls.insert({"bindRoot", bindRootDynamic});

  unbindRoot.push_back({"LQ", {&UnbindRoot_type, &UnbindRoot}});
  instanceCalls.insert({"unbindRoot", unbindRoot});
  unbindRootDynamic.push_back({"LQ", (void *) &Var_unbindRoot});
  dynamicCalls.insert({"unbindRoot", unbindRootDynamic});

  return {{{(uint64_t) varType, dynamicCalls}}, {{(uint64_t) varType, instanceCalls}}};
}
