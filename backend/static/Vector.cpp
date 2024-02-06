#include "Vector.h"

using namespace std;
using namespace llvm;


ObjectTypeSet Transient_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return ObjectTypeSet(persistentVectorType);
}

TypedValue Transient(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("PersistentVector_transient", ObjectTypeSet(persistentVectorType), args);
}

ObjectTypeSet Persistent_BANG__type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return ObjectTypeSet(persistentVectorType);
}

TypedValue Persistent_BANG_(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("PersistentVector_persistent_BANG_", ObjectTypeSet(persistentVectorType), args);
}


ObjectTypeSet Assoc_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return ObjectTypeSet(persistentVectorType);
}

TypedValue Assoc(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 3) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto other = gen->box(args[2]);
  std::vector<TypedValue> updatedArgs = {args[0], args[1], other};
  return gen->callRuntimeFun("PersistentVector_assoc", ObjectTypeSet(persistentVectorType), updatedArgs);
}

ObjectTypeSet Assoc_BANG__type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return ObjectTypeSet(persistentVectorType);
}

TypedValue Assoc_BANG_(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 3) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto other = gen->box(args[2]);
  std::vector<TypedValue> updatedArgs = {args[0], args[1], other};
  return gen->callRuntimeFun("PersistentVector_assoc_BANG_", ObjectTypeSet(persistentVectorType), updatedArgs);
}


ObjectTypeSet Conj_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return ObjectTypeSet(persistentVectorType);
}

TypedValue Conj(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto other = gen->box(args[1]);
  std::vector<TypedValue> updatedArgs = {args[0], other};
  return gen->callRuntimeFun("PersistentVector_conj", ObjectTypeSet(persistentVectorType), updatedArgs);
}

ObjectTypeSet Conj_BANG__type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return ObjectTypeSet(persistentVectorType);
}

TypedValue Conj_BANG_(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto other = gen->box(args[1]);
  std::vector<TypedValue> updatedArgs = {args[0], other};
  return gen->callRuntimeFun("PersistentVector_conj_BANG_", ObjectTypeSet(persistentVectorType), updatedArgs);
}


ObjectTypeSet Pop_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return ObjectTypeSet(persistentVectorType);
}

TypedValue Pop(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("PersistentVector_pop", ObjectTypeSet(persistentVectorType), args);
}

ObjectTypeSet Pop_BANG__type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return ObjectTypeSet(persistentVectorType);
}

TypedValue Pop_BANG_(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("PersistentVector_pop_BANG_", ObjectTypeSet(persistentVectorType), args);
}

pair<unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>>,
     unordered_map<string, // className
       unordered_map<string, // methodName
         vector<
           pair<
             string,
             pair<
               StaticCallType,
               StaticCall>>>>>> getVectorStaticAndInstanceFunctions() {
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> staticCalls;
  vector<pair<string, pair<StaticCallType, StaticCall>>> assoc, conj, pop;
  
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> instanceCalls;
  vector<pair<string, pair<StaticCallType, StaticCall>>> transient, persistent_BANG_, assoc_BANG_, conj_BANG_, pop_BANG_;

  vector<string> types {"J", "D", "Z", "LS", "LV", "LL", "LY", "LK", "LF", "LN", "LO"};
  
  transient.push_back({"LV", {&Transient_type, &Transient}});
  instanceCalls.insert({"asTransient", transient});
  
  persistent_BANG_.push_back({"LV", {&Persistent_BANG__type, &Persistent_BANG_}});
  instanceCalls.insert({"persistent", persistent_BANG_});
  
  // TODO: Rewrite definitions after fixing type checker
  // assoc.push_back({"LVJLO", {&Assoc_type, &Assoc}});
  // vals.insert({"clojure.lang.RT/assoc", assoc});
  for (auto type: types) assoc.push_back({"LVJ" + type, {&Assoc_type, &Assoc}});
  staticCalls.insert({"clojure.lang.RT/assoc", assoc});
  
  for (auto type: types) assoc_BANG_.push_back({"LVJ" + type, {&Assoc_BANG__type, &Assoc_BANG_}});
  instanceCalls.insert({"assoc", assoc_BANG_});
  
  for (auto type: types) conj.push_back({"LV" + type, {&Conj_type, &Conj}});
  staticCalls.insert({"clojure.lang.RT/conj", conj});
  
  for (auto type: types) conj_BANG_.push_back({"LV" + type, {&Conj_BANG__type, &Conj_BANG_}});
  instanceCalls.insert({"conj", conj_BANG_});
  
  pop.push_back({"LV", {&Pop_type, &Pop}});
  staticCalls.insert({"clojure.lang.RT/pop", pop});
  
  pop_BANG_.push_back({"LV", {&Pop_BANG__type, &Pop_BANG_}});
  instanceCalls.insert({"pop", pop_BANG_});
  
  return {staticCalls, {{"vector", instanceCalls}}};
}
