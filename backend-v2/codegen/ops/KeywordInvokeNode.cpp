#include "../CodeGen.h"

using namespace llvm;
using namespace std;

namespace rt {

ObjectTypeSet CodeGen::getType(const Node &node, const clojure::rt::protobuf::bytecode::KeywordInvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet::dynamicType().restriction(typeRestrictions);
}

TypedValue CodeGen::codegen(const Node &node, const clojure::rt::protobuf::bytecode::KeywordInvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // Evaluate the keyword
  TypedValue keywordVal = codegen(subnode.keyword(), ObjectTypeSet::all());
  guard.push(keywordVal); // Note: generateStaticKeywordInvoke borrows keywordVal and does not consume it!
  
  // Evaluate the target 
  TypedValue targetVal = codegen(subnode.target(), ObjectTypeSet::all());
  guard.push(targetVal);

  // Call the existing highly-optimized keyword invoke 
  // It natively compiles to Keyword_invoke -> PersistentArrayMap_get
  TypedValue result = invokeManager.generateStaticKeywordInvoke(keywordVal, {targetVal}, &guard, &node);

  // Release the borrowed keyword object
  memoryManagement.dynamicRelease(keywordVal);

  return TypedValue(result.type.restriction(typeRestrictions), result.value);
}

} // namespace rt
