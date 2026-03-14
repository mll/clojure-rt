#ifndef DYNAMIC_CONSTRUCTOR_H
#define DYNAMIC_CONSTRUCTOR_H

#include "LLVMTypes.h"
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "invoke/InvokeManager.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

namespace rt {

class ShadowStackGuard;

class DynamicConstructor {
private:
  LLVMTypes &types;
  InvokeManager &invokeManager;
  std::vector<RTValue> &generatedConstants;

public:
  explicit DynamicConstructor(LLVMTypes &t, InvokeManager &i,
                              std::vector<RTValue> &gc);
  /*
   *  Always tries to create an unboxed value, even for pointers.
   *  Only nil cannot be created this way as it exists only in boxed state.
   *  The argument s is never needed afterwards
   */

  TypedValue createDouble(const char *s);
  TypedValue createDouble(const double d);
  TypedValue createInt32(const char *s);
  TypedValue createInt32(const int32_t i);
  TypedValue createNil();
  TypedValue createBoolean(const char *s);
  TypedValue createBoolean(const bool b);
  TypedValue createString(const char *s);
  TypedValue createKeyword(const char *s);
  TypedValue createSymbol(const char *s);
  TypedValue createBigInteger(const char *s);
  TypedValue createRatio(const char *s);

  /*
   * It is currently assumed the runtime constructors never throw
   */
  TypedValue createVector(std::vector<TypedValue> &items,
                          ShadowStackGuard *guard = nullptr);
  TypedValue createArrayMap(std::vector<TypedValue> &keys,
                            std::vector<TypedValue> &values,
                            ShadowStackGuard *guard = nullptr);
  TypedValue createList(std::vector<TypedValue> &items,
                        ShadowStackGuard *guard = nullptr);
};

} // namespace rt

#endif // VALUE_ENCODER_H
