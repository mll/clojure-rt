#include "String.h"  
#include <math.h>

using namespace std;
using namespace llvm;

extern "C" {
  typedef struct String String;
  BOOL String_contains(String *self, String *other);
  int64_t String_indexOf(String *self, String *other);
  int64_t String_indexOfFrom(String *self, String *other, int64_t fromIndex);
  String *String_replace(String *self, String *target, String *replacement);
}

ObjectTypeSet Contains_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 2)
    throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  for (int i = 0; i < 2; ++i)
    if (!args[i].contains(stringType))
      throw CodeGenerationException("Expected a string, instead got " + args[i].toString(), node);
  return ObjectTypeSet(booleanType);
}

TypedValue Contains(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2)
    throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  for (int i = 0; i < 2; ++i)
    if (!args[i].first.contains(stringType))
      throw CodeGenerationException("Expected a string, instead got " + args[i].first.toString(), node);
  return gen->callRuntimeFun("String_contains", ObjectTypeSet(booleanType), args);
}

ObjectTypeSet Print_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return ObjectTypeSet(nilType);
}

TypedValue Print(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 1) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);

  auto toString = gen->callRuntimeFun("Object_toString", ObjectTypeSet(stringType), {gen->box(args[0])});
  auto compactify = gen->callRuntimeFun("String_compactify", ObjectTypeSet(stringType), {toString});
  auto cStr = gen->callRuntimeFun("String_c_str", Type::getInt8Ty(*(gen->TheContext))->getPointerTo(), {Type::getInt8Ty(*(gen->TheContext))->getPointerTo()}, {compactify.second});
  auto res = gen->callRuntimeFun("printf", Type::getInt64Ty(*(gen->TheContext)), {Type::getInt8Ty(*(gen->TheContext))->getPointerTo()}, 
                                 {gen->Builder->CreateGlobalStringPtr(StringRef("String/print: '%s'\n"), "dynamicString"), cStr}, true);  
  gen->dynamicRelease(compactify.second, false);
  return {ObjectTypeSet(integerType), res};
}


// Many other things can be derefed, but for now it is only var
ObjectTypeSet IndexOf_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 2 && args.size() != 3)
    throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  for (int i = 0; i < 2; ++i)
    if (!args[i].contains(stringType))
      throw CodeGenerationException("Expected a string, instead got " + args[i].toString(), node);
  if (args.size() == 3 && !args[2].contains(integerType))
    throw CodeGenerationException("Expected an integer, instead got " + args[2].toString(), node);
  return ObjectTypeSet(integerType);
}

TypedValue IndexOf(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2 && args.size() != 3)
    throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  for (int i = 0; i < 2; ++i)
    if (!args[i].first.contains(stringType))
      throw CodeGenerationException("Expected a string, instead got " + args[i].first.toString(), node);
  std::vector<TypedValue> updatedArgs = {args[0], args[1]};
  if (args.size() == 3) { // TODO: Determined here?
    if (!args[2].first.contains(integerType))
      throw CodeGenerationException("Expected an integer, instead got " + args[2].first.toString(), node);
    updatedArgs.push_back(gen->unbox(args[2]));
    return gen->callRuntimeFun("String_indexOfFrom", ObjectTypeSet(integerType), updatedArgs);
  } else {
    return gen->callRuntimeFun("String_indexOf", ObjectTypeSet(integerType), updatedArgs);
  }
}

ObjectTypeSet Replace_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 3)
    throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  for (int i = 0; i < 3; ++i)
    if (!args[i].contains(stringType))
      throw CodeGenerationException("Expected a string, instead got " + args[i].toString(), node);
  return ObjectTypeSet(stringType);
}

TypedValue Replace(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 3)
    throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  return gen->callRuntimeFun("String_replace", ObjectTypeSet(stringType), args);
}

pair<
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>>,

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
          std::pair<StaticCallType, StaticCall>>>>>>>
getStringStaticFunctions() {
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> staticCalls;

  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> instanceCalls;
  vector<pair<string, pair<StaticCallType, StaticCall>>> contains, indexOf, replace, print;
  
  unordered_map<string, vector<pair<string, void *>>> dynamicCalls;
  vector<pair<string, void *>> containsDynamic, indexOfDynamic, replaceDynamic;

  contains.push_back({"LSLS", {&Contains_type, &Contains}});

  print.push_back({"LV", {&Print_type, &Print}});
  print.push_back({"LN", {&Print_type, &Print}});
  print.push_back({"LL", {&Print_type, &Print}});
  print.push_back({"LK", {&Print_type, &Print}});
  print.push_back({"LI", {&Print_type, &Print}});
  print.push_back({"LR", {&Print_type, &Print}});
  print.push_back({"LA", {&Print_type, &Print}});
  print.push_back({"LS", {&Print_type, &Print}});
  print.push_back({"D", {&Print_type, &Print}});
  print.push_back({"J", {&Print_type, &Print}});
  print.push_back({"Z", {&Print_type, &Print}});

  instanceCalls.insert({"contains", contains});
  staticCalls.insert({"java.lang.String/print", print});

  containsDynamic.push_back({"LSLS", (void *) &String_contains});
  dynamicCalls.insert({"contains", containsDynamic});

  indexOf.push_back({"LSLS", {&IndexOf_type, &IndexOf}});
  indexOf.push_back({"LSLSJ", {&IndexOf_type, &IndexOf}});
  instanceCalls.insert({"indexOf", indexOf});
  indexOfDynamic.push_back({"LSLS", (void *) &String_indexOf});
  indexOfDynamic.push_back({"LSLSJ", (void *) &String_indexOfFrom});
  dynamicCalls.insert({"indexOf", indexOfDynamic});

  replace.push_back({"LSLSLS", {&Replace_type, &Replace}});
  instanceCalls.insert({"replace", replace});
  replaceDynamic.push_back({"LSLSLS", (void *) &String_replace});
  dynamicCalls.insert({"replace", replaceDynamic});

  return {staticCalls, {{{(uint64_t) stringType, dynamicCalls}}, {{(uint64_t) stringType, instanceCalls}}}};
}
