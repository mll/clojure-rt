(ns clojure.intrinsics)

'{java.lang.Object
  {:instance-fns
   {toString [{:args [] :type :call :symbol "toString" :returns java.lang.String}]}}

  java.lang.Class 
  {:inherits-from java.lang.Object}
  
  java.lang.Long 
  {:inherits-from java.lang.Object
   :static-fields
   {BYTES 4
    MAX_VALUE 2147483647
    MIN_VALUE -2147483647
    SIZE 32}}

  java.lang.Double
  {:inherits-from java.lang.Object}

  java.lang.PersistentArrayMap
  {:inherits-from java.lang.Object}

  clojure.lang.BigInt
  {:inherits-from java.lang.Object}

  clojure.lang.Ratio
  {:inherits-from java.lang.Object}
  
  clojure.lang.Boolean
  {:inherits-from java.lang.Object}

  clojure.lang.Keyword
  {:inherits-from java.lang.Object}

  clojure.lang.Symbol
  {:inherits-from java.lang.Object}

  clojure.asm.Opcodes
  {:is-interface true
   :static-fields
   {ACC_PUBLIC       0x1      ACC_PRIVATE      0x2      ACC_PROTECTED    0x4
    ACC_STATIC       0x8      ACC_FINAL        0x10     ACC_SUPER        0x20
    ACC_SYNCHRONIZED 0x20     ACC_OPEN         0x20     ACC_TRANSITIVE   0x20
    ACC_VOLATILE     0x40     ACC_BRIDGE       0x40     ACC_STATIC_PHASE 0x40
    ACC_VARARGS      0x80     ACC_TRANSIENT    0x80     ACC_NATIVE       0x100
    ACC_INTERFACE    0x200    ACC_ABSTRACT     0x400    ACC_STRICT       0x800
    ACC_SYNTHETIC    0x1000   ACC_ANNOTATION   0x2000   ACC_ENUM         0x4000
    ACC_MANDATED     0x8000   ACC_MODULE       0x8000
    
    V1_1 0x3002D   V1_2 0x2E   V1_3 0x2F   V1_4 0x30   V1_5 0x31
    V1_6 0x32      V1_7 0x33   V1_8 0x34   V9   0x35   V10  0x36   V11 0x37}}
  
  ;; character: clojure.lang.Character

  clojure.lang.Var
  {:inherits-from java.lang.Object}


  clojure.lang.Numbers
  {:static-fns
   {add   [{:args [:double :double] :type :intrinsic :symbol "FAdd"         :returns :double}
           {:args [:int :int]       :type :intrinsic :symbol "Add"          :returns :int}
           {:args [:any :any]       :type :call      :symbol "Object_add"   :returns :any}]
    minus [{:args [:double :double] :type :intrinsic :symbol "FSub"         :returns :double}
           {:args [:int :int]       :type :intrinsic :symbol "Sub"          :returns :int}
           {:args [:any :any]       :type :call      :symbol "sub"          :returns :any}]
    multiply [{:args [:double :double] :type :intrinsic :symbol "FMul"         :returns :double}
              {:args [:int :int]       :type :intrinsic :symbol "Mul"          :returns :int}
              {:args [:any :any]       :type :call      :symbol "mul"          :returns :any}]
    divide  [{:args [:double :double] :type :intrinsic :symbol "FDiv"          :returns :double}
             {:args [:int :int]       :type :intrinsic :symbol "Div"           :returns :int}
             {:args [:any :any]       :type :call      :symbol "div"          :returns :any}]
    gte  [{:args [:double :double] :type :intrinsic :symbol "FCmpOGE"         :returns :bool}
          {:args [:int :int]       :type :intrinsic :symbol "ICmpSGE"         :returns :bool}
          {:args [:any :any]       :type :call      :symbol "gte"             :returns :bool}]
    lt  [{:args [:double :double] :type :intrinsic :symbol "FCmpOLT"          :returns :double}
         {:args [:int :int]       :type :intrinsic :symbol "ICmpOLT"          :returns :int}
         {:args [:any :any]       :type :call      :symbol "lt"               :returns :bool}]}}

  java.lang.Math
  {:static-fns
   {sin   [{:args [:double] :type :call :symbol "sin"   :returns :double}]
    cos   [{:args [:double] :type :call :symbol "cos"   :returns :double}]
    tan   [{:args [:double] :type :call :symbol "tan"   :returns :double}]
    asin  [{:args [:double] :type :call :symbol "asin"  :returns :double}]
    acos  [{:args [:double] :type :call :symbol "acos"  :returns :double}]
    atan  [{:args [:double] :type :call :symbol "atan"  :returns :double}]
    exp   [{:args [:double] :type :call :symbol "exp"   :returns :double}]
    exp2  [{:args [:double] :type :call :symbol "exp2"  :returns :double}]
    exp10 [{:args [:double] :type :call :symbol "exp10" :returns :double}]
    log   [{:args [:double] :type :call :symbol "log"   :returns :double}]
    log10 [{:args [:double] :type :call :symbol "log10" :returns :double}]
    logb  [{:args [:double] :type :call :symbol "logb"  :returns :double}]
    log2  [{:args [:double] :type :call :symbol "log2"  :returns :double}]
    sqrt  [{:args [:double] :type :call :symbol "sqrt"  :returns :double}]
    cbrt  [{:args [:double] :type :call :symbol "cbrt"  :returns :double}]
    exp1m [{:args [:double] :type :call :symbol "exp1m" :returns :double}]
    log1p [{:args [:double] :type :call :symbol "log1p" :returns :double}]
    abs   [{:args [:double] :type :call :symbol "fabs"  :returns :double}]
    atan2 [{:args [:double :double] :type :call :symbol "atan2" :returns :double}]
    pow   [{:args [:double :double] :type :call :symbol "pow"   :returns :double}]
    hypot [{:args [:double :double] :type :call :symbol "hypot" :returns :double}]}}

  java.lang.String
  {:static-fns
   {print [{:args [:any] :type :call :symbol "print" :returns :nil}]}
   :instance-fns
   {contains [{:args [java.lang.String] :type :call :symbol "String_contains" :returns :bool}]
    indexOf  [{:args [java.lang.String] :type :call :symbol "String_indexOf"  :returns :int}
              {:args [java.lang.String :int] :type :call :symbol "String_indexOfFrom"  :returns :int}]
    replace [{:args [java.lang.String java.lang.String] :type :call :symbol "String_replace" :returns java.lang.String}]}}

  clojure.lang.Util
  {:static-fns
   {equiv [{:args [:int :int] :type :intrinsic :symbol "ICmpEQ" :returns :bool}
           {:args [:double :double] :type :intrinsic :symbol "FCmpOEQ" :returns :bool}
           {:args [:any :any] :type :call :symbol "equals" :returns :bool}]}}

  clojure.lang.PersistentVector
  {:instance-fns 
   {asTransient [{:args [] :type :call :symbol "PersistentVector_transient" :returns clojure.lang.PersistentVector}]
    persistent [{:args [] :type :call :symbol "PersistentVector_persistent_BANG_" :returns clojure.lang.PersistentVector}]
    conj [{:args [:any] :type :call :symbol "PersistentVector_conj_BANG_" :returns clojure.lang.PersistentVector}]
    assoc [{:args [:int :any] :type :call :symbol "PersistentVector_assoc_BANG_" :returns clojure.lang.PersistentVector}]
    pop [{:args [clojure.lang.PersistentVector] :type :call :symbol "PersistentVector_pop_BANG_" :returns clojure.lang.PersistentVector}]}}

  clojure.lang.RT
  {:static-fns 
   {conj [{:args [clojure.lang.PersistentVector :any] :type :call :symbol "PersistentVector_conj" :returns clojure.lang.PersistentVector}]
    assoc [{:args [clojure.lang.PersistentVector :int :any] :type :call :symbol "PersistentVector_assoc" :returns clojure.lang.PersistentVector}]
    pop [{:args [clojure.lang.PersistentVector] :type :call :symbol "PersistentVector_pop" :returns clojure.lang.PersistentVector}]}}}

