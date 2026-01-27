#ifndef CONSTANT_RATIO_H
#define CONSTANT_RATIO_H

#include <gmp.h>
#include "ObjectTypeConstant.h"

namespace rt {

class ConstantRatio: public ObjectTypeConstant {
  public:
  mpq_t value;
  ConstantRatio(mpq_t val) : ObjectTypeConstant(ratioType) { 
    mpq_init(value);
    mpq_set(value, val);
  }
  ConstantRatio(std::string val) : ObjectTypeConstant(ratioType) {
    mpq_init(value);
    assert(mpq_set_str(value, val.c_str(), 10) == 0 && "Failed to initialize Ratio");
    mpq_canonicalize(value);
  }
  ConstantRatio(int32_t num, int32_t den) : ObjectTypeConstant(ratioType) {
    mpq_init(value);
    mpq_set_si(value, num, den);
    mpq_canonicalize(value);
  }
  ConstantRatio(mpz_t num, mpz_t den) : ObjectTypeConstant(ratioType) {
    mpq_init(value);
    mpq_set_z(value, num);
    mpz_clear(num);
    mpq_t den2;
    mpq_init(den2);
    mpq_set_z(den2, den);
    mpz_clear(den);
    mpq_div(value, value, den2);
    mpq_clear(den2);
  }
  ~ConstantRatio() {
    mpq_clear(value);
  }
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantRatio(value)); }
  virtual std::string toString() { return std::string(mpq_get_str(NULL, 10, value)); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantRatio *i = dynamic_cast<ConstantRatio *>(other)) {
      return mpq_equal(i->value, value);
    }
    return false;
  }
};

} // namespace rt

#endif
