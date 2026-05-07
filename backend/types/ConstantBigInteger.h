#ifndef CONSTANT_BIG_INTEGER_H
#define CONSTANT_BIG_INTEGER_H

#include <gmp.h>
#include <cassert>
#include "ObjectTypeConstant.h"

namespace rt {

class ConstantBigInteger: public ObjectTypeConstant {
  public:
  mpz_t value;
  ConstantBigInteger(mpz_t val) : ObjectTypeConstant(bigIntegerType) {
    mpz_init_set(value, val);
  }
  ConstantBigInteger(mpq_t val) : ObjectTypeConstant(bigIntegerType) { 
    assert(mpz_cmp_si(mpq_denref(val), 1) == 0);
    mpz_init_set(value, mpq_numref(val));
  }
  ConstantBigInteger(std::string val) : ObjectTypeConstant(bigIntegerType) {
    assert(mpz_init_set_str(value, val.c_str(), 10) == 0 && "Failed to initialize BigInteger");
  }
  ~ConstantBigInteger() {
    mpz_clear(value);
  }
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantBigInteger(value)); }
  virtual std::string toString() {
    char *str = mpz_get_str(NULL, 10, value);
    std::string s(str);
    free(str);
    return s;
  }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantBigInteger *i = dynamic_cast<ConstantBigInteger *>(other)) {
      return mpz_cmp(i->value, value) == 0;
    }
    return false;
  }
};

} // namespace rt

#endif
