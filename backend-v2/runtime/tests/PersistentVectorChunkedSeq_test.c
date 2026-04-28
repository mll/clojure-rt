#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "TestTools.h"
#include "../BigInteger.h"
#include "../Function.h"
#include "../Integer.h"
#include "../PersistentList.h"

static RTValue MockBigIntAddition(Frame *frame, RTValue a, RTValue b, RTValue a2,
                                   RTValue a3, RTValue a4) {
  BigInteger *valA = (BigInteger *)RT_unboxPtr(a);
  BigInteger *valB = (BigInteger *)RT_unboxPtr(b);
  // retain because BigInteger_add will consume them, 
  // but RT_invokeMethodWithFrame will ALSO release them later.
  Ptr_retain(valA);
  Ptr_retain(valB);
  BigInteger *res = BigInteger_add(valA, valB);
  return RT_boxPtr(res);
}

static RTValue create_bigint_add_fn() {
  ClojureFunction *f = Function_create(1, 2, false);
  Function_fillMethod(f, 0, 0, 2, false, MockBigIntAddition, "add", 0);
  return RT_boxPtr(f);
}

static RTValue MockAddition(Frame *frame, RTValue a, RTValue b, RTValue a2,
                            RTValue a3, RTValue a4) {
  int32_t valA = RT_unboxInt32(a);
  int32_t valB = RT_unboxInt32(b);
  release(a);
  release(b);
  return RT_boxInt32(valA + valB);
}

static RTValue create_mock_add_fn() {
  ClojureFunction *f = Function_create(1, 2, false);
  Function_fillMethod(f, 0, 0, 2, false, MockAddition, "add", 0);
  return RT_boxPtr(f);
}

static void test_chunked_seq_basic(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 10; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }

    RTValue seqVal = PersistentVector_seq(v); // v consumed
    assert_true(RT_isPtr(seqVal));
    Object *obj = (Object *)RT_unboxPtr(seqVal);
    assert_int_equal(obj->type, persistentVectorChunkedSeqType);

    PersistentVectorChunkedSeq *seq = (PersistentVectorChunkedSeq *)obj;

    // Test count
    Ptr_retain(seq);
    int32_t count = PersistentVectorChunkedSeq_count(seq);
    assert_int_equal(count, 10);

    // Iterate
    for (int i = 0; i < 10; i++) {
      Ptr_retain(seq);
      RTValue first = PersistentVectorChunkedSeq_first(seq);
      assert_int_equal(RT_unboxInt32(first), i);
      release(first);

      RTValue next = PersistentVectorChunkedSeq_next(seq); // consumes seq
      if (i < 9) {
        assert_true(RT_isPtr(next));
        seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(next);
      } else {
        assert_true(RT_isNil(next));
      }
    }
  });
}

static void test_chunked_seq_large(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 100; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }

    RTValue seqVal = PersistentVector_seq(v);
    PersistentVectorChunkedSeq *seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(seqVal);

    for (int i = 0; i < 100; i++) {
      Ptr_retain(seq);
      RTValue first = PersistentVectorChunkedSeq_first(seq);
      assert_int_equal(RT_unboxInt32(first), i);
      release(first);

      RTValue next = PersistentVectorChunkedSeq_next(seq);
      if (i < 99) {
        seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(next);
      } else {
        assert_true(RT_isNil(next));
      }
    }
  });
}

static void test_chunked_seq_drop(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 100; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }

    RTValue seqVal = PersistentVector_seq(v);
    PersistentVectorChunkedSeq *seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(seqVal);

    // Drop 50
    RTValue droppedVal = PersistentVectorChunkedSeq_drop(seq, 50); // consumes seq
    PersistentVectorChunkedSeq *dropped = (PersistentVectorChunkedSeq *)RT_unboxPtr(droppedVal);

    Ptr_retain(dropped);
    assert_int_equal(PersistentVectorChunkedSeq_count(dropped), 50);

    Ptr_retain(dropped);
    RTValue first = PersistentVectorChunkedSeq_first(dropped);
    assert_int_equal(RT_unboxInt32(first), 50);
    release(first);

    Ptr_release(dropped);
  });
}

static void test_chunked_seq_chunked_first(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 64; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }

    RTValue seqVal = PersistentVector_seq(v);
    PersistentVectorChunkedSeq *seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(seqVal);

    // Chunked first should return a chunk of size 32 (if it's at the beginning)
    RTValue chunkVal = PersistentVectorChunkedSeq_chunkedFirst(seq); // consumes seq
    ArrayChunk *chunk = (ArrayChunk *)RT_unboxPtr(chunkVal);

    Ptr_retain(chunk);
    assert_int_equal(ArrayChunk_count(chunk), 32);

    for (int i = 0; i < 32; i++) {
      Ptr_retain(chunk);
      RTValue val = ArrayChunk_nth(chunk, i);
      assert_int_equal(RT_unboxInt32(val), i);
      release(val);
    }

    Ptr_release(chunk);
  });
}

static void test_chunked_seq_reduce(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    int count = 1000;
    for (int i = 0; i < count; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }

    RTValue seqVal = PersistentVector_seq(v);
    PersistentVectorChunkedSeq *seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(seqVal);

    RTValue addFn = create_mock_add_fn();
    RTValue result = PersistentVectorChunkedSeq_reduce(seq, addFn, RT_boxInt32(0));

    assert_int_equal(RT_unboxInt32(result), (count * (count - 1)) / 2); 
    release(result);
  });
}

static void test_vector_reduce(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    int count = 1000;
    for (int i = 0; i < count; i++) {
      v = PersistentVector_conj(v, RT_boxPtr(BigInteger_createFromInt(i)));
    }

    RTValue addFn = create_bigint_add_fn();
    RTValue result = PersistentVector_reduce(v, addFn, RT_boxPtr(BigInteger_createFromInt(0)));

    BigInteger *expected = BigInteger_createFromInt((count * (count - 1)) / 2);
    assert_true(BigInteger_equals((BigInteger *)RT_unboxPtr(result), expected));
    
    release(result);
    Ptr_release(expected);
  });
}

static void test_vector_reduce_large(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    int count = 10000;
    for (int i = 0; i < count; i++) {
      v = PersistentVector_conj(v, RT_boxPtr(BigInteger_createFromInt(i)));
    }

    RTValue addFn = create_bigint_add_fn();
    RTValue result = PersistentVector_reduce(v, addFn, RT_boxPtr(BigInteger_createFromInt(0)));

    BigInteger *expected = BigInteger_createFromInt(((int64_t)count * (count - 1)) / 2);
    assert_true(BigInteger_equals((BigInteger *)RT_unboxPtr(result), expected));
    
    release(result);
    Ptr_release(expected);
  });
}

static void test_vector_reduce_edge_cases(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    // 0 elements
    PersistentVector *v0 = PersistentVector_create();
    RTValue res0 = PersistentVector_reduce(v0, create_mock_add_fn(), RT_boxInt32(100));
    assert_int_equal(RT_unboxInt32(res0), 100);
    release(res0);

    // 1 element
    PersistentVector *v1 = PersistentVector_create();
    v1 = PersistentVector_conj(v1, RT_boxInt32(42));
    RTValue res1 = PersistentVector_reduce(v1, create_mock_add_fn(), RT_boxInt32(0));
    assert_int_equal(RT_unboxInt32(res1), 42);
    release(res1);

    // 32 elements (full tail, no root)
    PersistentVector *v32 = PersistentVector_create();
    for (int i = 0; i < 32; i++) {
      v32 = PersistentVector_conj(v32, RT_boxInt32(i));
    }
    RTValue res32 = PersistentVector_reduce(v32, create_mock_add_fn(), RT_boxInt32(0));
    assert_int_equal(RT_unboxInt32(res32), (32 * 31) / 2);
    release(res32);
  });
}

static void test_chunked_seq_reentrancy(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    int count = 1000;
    for (int i = 0; i < count; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }

    RTValue seqVal = PersistentVector_seq(v);
    PersistentVectorChunkedSeq *seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(seqVal);

    // Keep seq alive (count becomes 2)
    Ptr_retain(seq);

    RTValue nextVal = PersistentVectorChunkedSeq_next(seq); // consumes one reference
    PersistentVectorChunkedSeq *next = (PersistentVectorChunkedSeq *)RT_unboxPtr(nextVal);

    // next should be a NEW object because seq was shared
    assert_ptr_not_equal(seq, next);

    // Now consume seq (count is 1)
    RTValue next2Val = PersistentVectorChunkedSeq_next(seq);

    // next2 might be the same as next? No, next was created earlier.
    // But if we do next on next:
    Ptr_retain(next);
    RTValue next3Val = PersistentVectorChunkedSeq_next(next);
    assert_ptr_not_equal(next, RT_unboxPtr(next3Val));
    release(next3Val);

    // If next is count 1, it should update in-place
    RTValue next4Val = PersistentVectorChunkedSeq_next(next); // consumes next (count was 1)
    assert_ptr_equal(next, RT_unboxPtr(next4Val));

    release(next4Val);
    release(next2Val);
  });
}

static void test_chunked_seq_chunked_more(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 64; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }

    RTValue seqVal = PersistentVector_seq(v);
    PersistentVectorChunkedSeq *seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(seqVal);

    // chunkedMore should return a seq starting at the next chunk
    RTValue moreVal = PersistentVectorChunkedSeq_chunkedMore(seq); // consumes seq
    assert_true(RT_isPtr(moreVal));
    PersistentVectorChunkedSeq *more = (PersistentVectorChunkedSeq *)RT_unboxPtr(moreVal);

    // Verify it starts at 32
    Ptr_retain(more);
    RTValue firstVal = PersistentVectorChunkedSeq_first(more);
    assert_int_equal(RT_unboxInt32(firstVal), 32);
    release(firstVal);

    // Now chunkedMore on this should be empty (since there are no more chunks)
    RTValue evenMoreVal = PersistentVectorChunkedSeq_chunkedMore(more); // consumes more
    assert_true(RT_isPtr(evenMoreVal));
    
    // It should be an empty list (PersistentList_empty())
    assert_int_equal(((Object *)RT_unboxPtr(evenMoreVal))->type, persistentListType);
    assert_int_equal(PersistentList_count((PersistentList *)RT_unboxPtr(evenMoreVal)), 0);
  });
}

static void test_empty_vector_seq(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    RTValue seq = PersistentVector_seq(v);
    assert_true(RT_isNil(seq));
  });
}

static void test_chunked_seq_toString(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 3; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }

    RTValue seqVal = PersistentVector_seq(v);
    PersistentVectorChunkedSeq *seq = (PersistentVectorChunkedSeq *)RT_unboxPtr(seqVal);

    String *s = PersistentVectorChunkedSeq_toString(seq); // consumes seq
    s = String_compactify(s); // Manual compactify for verification
    const char *expected = "(0 1 2)";
    assert_string_equal(String_c_str(s), expected);
    Ptr_release(s);
  });
}


int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_chunked_seq_basic),
      cmocka_unit_test(test_chunked_seq_large),
      cmocka_unit_test(test_chunked_seq_drop),
      cmocka_unit_test(test_chunked_seq_chunked_first),
      cmocka_unit_test(test_chunked_seq_reduce),
      cmocka_unit_test(test_vector_reduce),
      cmocka_unit_test(test_vector_reduce_large),
      cmocka_unit_test(test_vector_reduce_edge_cases),
      cmocka_unit_test(test_chunked_seq_reentrancy),
      cmocka_unit_test(test_chunked_seq_chunked_more),
      cmocka_unit_test(test_empty_vector_seq),
      cmocka_unit_test(test_chunked_seq_toString),
  };

  initialise_memory();
  RuntimeInterface_initialise();
  return cmocka_run_group_tests(tests, NULL, NULL);
}
