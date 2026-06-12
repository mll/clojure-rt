#include "TestTools.h"
#include "../PersistentVectorReverseSeq.h"
#include "../Integer.h"
#include <cmocka.h>

static void test_persistent_vector_reverse_seq_basic(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    v = PersistentVector_conj(v, RT_boxInt32(10));
    v = PersistentVector_conj(v, RT_boxInt32(20));
    v = PersistentVector_conj(v, RT_boxInt32(30));

    RTValue seq = PersistentVectorReverseSeq_create(v, 2);
    PersistentVectorReverseSeq *rs = (PersistentVectorReverseSeq *)RT_unboxPtr(seq);
    
    // first should be 30
    Ptr_retain(rs);
    RTValue f = PersistentVectorReverseSeq_first(rs);
    assert_int_equal(RT_unboxInt32(f), 30);
    release(f);

    Ptr_retain(rs);
    assert_int_equal(PersistentVectorReverseSeq_count(rs), 3);

    Ptr_retain(rs);
    String *str = PersistentVectorReverseSeq_toString(rs);
    str = String_compactify(str);
    assert_string_equal(String_c_str(str), "(30 20 10)");
    Ptr_release(str);

    // next
    Ptr_retain(rs);
    RTValue nextSeq = PersistentVectorReverseSeq_next(rs);
    PersistentVectorReverseSeq *nrs = (PersistentVectorReverseSeq *)RT_unboxPtr(nextSeq);
    
    Ptr_retain(nrs);
    f = PersistentVectorReverseSeq_first(nrs);
    assert_int_equal(RT_unboxInt32(f), 20);
    release(f);

    // nrs had 1 refcount from creation. Ptr_retain(nrs) made it 2. _first consumed 1. We must release the last 1.
    release(nextSeq);
    release(seq);
    Ptr_release(v);
  });
}

static void test_persistent_vector_reverse_seq_empty_or_bounds(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    RTValue seq = PersistentVectorReverseSeq_create(v, -1);
    assert_true(seq == RT_boxNil());
    Ptr_release(v);
  });
}

static void test_persistent_vector_reverse_seq_unimplemented(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    v = PersistentVector_conj(v, RT_boxInt32(1));
    RTValue seq = PersistentVectorReverseSeq_create(v, 0);
    
    PersistentVectorReverseSeq *rs = (PersistentVectorReverseSeq *)RT_unboxPtr(seq);
    
    ASSERT_THROWS("IllegalArgumentException", {
      Ptr_retain(rs);
      PersistentVectorReverseSeq_cons(rs, RT_boxInt32(2));
    });

    release(seq);
    Ptr_release(v);
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_persistent_vector_reverse_seq_basic),
      cmocka_unit_test(test_persistent_vector_reverse_seq_empty_or_bounds),
      cmocka_unit_test(test_persistent_vector_reverse_seq_unimplemented),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
