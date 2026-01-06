(ns clojure.rt-protocols)

'{clojure.asm.Opcodes
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

  clojure.lang.IAtom
  {:is-interface true
   :instance-fns {swap [[:this :any] [:this :any :any] [:this :any :any :any] [:this :any :any :any :any]]
                  compareAndSwap [:this :any :any]
                  reset [:this :any]}}

  clojure.lang.IAtom2
  {:extends clojure.lang.IAtom
   :is-interface true
   :instance-fns {swapVals [[:this :any] [:this :any :any] [:this :any :any :any]]
                  resetVals [:this :any]}}

  clojure.lang.IBlockingDeref
  {:is-interface true
   :instance-fns {deref [:this :any :any]}}

  clojure.lang.Counted
  {:is-interface true
   :instance-fns {count [:this]}}

  clojure.lang.Indexed
  {:extends clojure.lang.Counted
   :is-interface true
   :instance-fns {count [:this]}}

  clojure.lang.IChunk
  {:extends clojure.lang.Indexed
   :is-interface true
   :instance-fns {dropFirst [:this]
                  reduce [:this :any]}}

  clojure.lang.Sequable
  {:is-interface true
   :instance-fns {seq [:this]}}

  clojure.lang.IPersistentCollection
  {:is-interface true
   :extends clojure.lang.Sequable
   :instance-fns {count [:this]
                  cons [:this :any]
                  empty [:this]
                  equiv [:this :any]}}

  clojure.lang.ISeq
  {:is-interface true
   :extends clojure.lang.IPersistentCollection
   :instance-fns {first [:this]
                  next [:this]
                  more [:this]
                  cons [:this :any]}}

  clojure.lang.IDeref
  {:is-interface true
   :instance-fns {deref [:this]}}

  clojure.lang.Sequential
  {:is-interface true}

  clojure.lang.IDrop
  {:is-interface true
   :extends clojure.lang.Sequential
   :instance-fns {drop [:this :any]}}

  clojure.lang.IExceptionInfo
  {:is-interface true
   :instance-fns {getData [:this]}}
  
  java.util.concurrent.Callable
  {:is-interface true
   :instance-fns {call [:this]}}
  
  java.lang.Runnable
  {:is-interface true
   :instance-fns {run [:this]}}
  
  clojure.lang.IFn 
  {:is-interface true
   :extends [java.util.concurrent.Callable java.lang.Runnable]
   :instance-fns {invoke [[:this] [:this :any] [:this :any :any] [:this :any :any :any] [:this :any :any :any :any] 
                         [:this :any :any :any :any :any] [:this :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any] [:this :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any] [:this :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any] 
                         [:this :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any :any]]
                  applyTo [:this :any]}}

  clojure.lang.IHashEq
  {:is-interface true
   :instance-fns {hasheq [:this]}}

  clojure.lang.IKVReduce 
  {:is-interface true
   :instance-fns {kvreduce [:this :any :any]}}

  clojure.lang.IKeywordLookup
  {:is-interface true
   :instance-fns {getLookupThunk [:this :any]}}

  clojure.lang.ILookup
  {:is-interface true
   :instance-fns {valAt [[:this :any] [:this :any :any]]}}
  
  clojure.lang.ILookupSite
  {:is-interface true
   :instance-fns {fault [:this :any]}}

  clojure.lang.ILookupThunk
  {:is-interface true
   :instance-fns {get [:this :any]}}

  java.util.Iterator
  {:is-interface true
   :instance-fns {forEachRemaining [:this :any]
                  hasNext [:this]
                  next [:this]
                  remove [:this]}}

  clojure.lang.IMapEntry
  {:is-interface true
   :instance-fns {key [:this]
                  val [:this]}}

  clojure.lang.IEditableCollection
  {:is-interface true
   :instance-fns {asTransient [:this]}}

  clojure.lang.IMeta
  {:is-interface true
   :instance-fns {meta [:this]}}

  clojure.lang.IObj
  {:is-interface true
   :extends clojure.lang.IMeta
   :instance-fns {withMeta [:this :any]}}

  clojure.lang.IPending
  {:is-interface true
   :instance-fns {isRealized [:this]}}

  clojure.lang.IPersistentStack
  {:is-interface true
   :extends clojure.lang.IPersistentCollection
   :instance-fns {peek [:this]
                  pop [:this]}}

  clojure.lang.IPersistentList
  {:is-interface true
   :extends [clojure.lang.Sequential clojure.lang.IPersistentStack]}

  clojure.lang.Associative
  {:is-interface true
   :extends [clojure.lang.IPersistentCollection clojure.lang.ILookup]
   :instance-fns {containsKey [:this :any]
                  entryAt [:this :any]
                  assoc [:this :any :any]}}

  java.lang.Iterable
  {:is-interface true
   :instance-fns {iterator [:this]
                  forEach [:this :any]
                  spliterator [:this]}}

  clojure.lang.IPersistentMap
  {:is-interface true
   :extends [java.lang.Iterable clojure.lang.Associative clojure.lang.Counted]
   :instance-fns {assoc [:this :any :any]
                  assocEx [:this :any :any]
                  without [:this :any]}}

  clojure.lang.IPersistentSet
  {:is-interface true
   :extends [clojure.lang.IPersistentCollection clojure.lang.Counted]
   :instance-fns {disjoin [:this :any]
                  contains [:this :any]
                  get [:this :any]}}

  clojure.lang.Reversible
  {:is-interface true
   :instance-fns {rseq [:this]}}

  clojure.lang.IPersistentVector
  {:is-interface true
   :extends [clojure.lang.Associative clojure.lang.Sequential clojure.lang.IPersistentStack clojure.lang.Reversible clojure.lang.Indexed]
   :instance-fns {assocN [:this :any :any]
                  cons [:this :any]}}

  clojure.lang.IProxy
  {:is-interface true
   :instance-fns {__initClojureFnMappings [:this :any]
                  __updateClojureFnMappings [:this :any]
                  __getClojureFnMappings [:this]}}

  clojure.lang.IRecord
  {:is-interface true}

  clojure.lang.IReduceInit
  {:is-interface true
   :instance-fns {reduce [:this :any :any]}}

  clojure.lang.IReduce
  {:is-interface true
   :extends clojure.lang.IReduceInit
   :instance-fns {reduce [:this :any]}}

  clojure.lang.IRef
  {:is-interface true
   :instance-fns {setValidator [:this :any]
                  getValidator [:this]
                  getWatchers [:this]
                  addWatch [:this :any :any]
                  removeWatch [:this :any]}}

  clojure.lang.IReference
  {:is-interface true
   :extends clojure.lang.IMeta
   :instance-fns {alterMeta [:this :any :any]
                  resetMeta [:this :any]}}

  clojure.lang.ITransientCollection
  {:is-interface true
   :instance-fns {conj [:this :any]
                  persistent [:this]}}

  clojure.lang.ITransientAssociative
  {:is-interface true
   :extends [clojure.lang.ITransientCollection clojure.lang.ILookup]
   :instance-fns {assoc [:this :any :any]}}

  clojure.lang.ITransientAssociative2
  {:is-interface true
   :extends clojure.lang.ITransientAssociative
   :instance-fns {containsKey [:this :any]
                  entryAt [:this :any]}}

  clojure.lang.ITransientMap
  {:is-interface true
   :extends [clojure.lang.ITransientAssociative clojure.lang.Counted]
   :instance-fns {assoc [:this :any :any]
                  without [:this :any]
                  persistent [:this]}}

  clojure.lang.ITransientSet
  {:is-interface true
   :extends [clojure.lang.ITransientCollection clojure.lang.Counted]
   :instance-fns {disjoin [:this :any]
                  contains [:this :any]
                  get [:this :any]}}

  clojure.lang.ITransientVector
  {:is-interface true
   :extends [clojure.lang.ITransientAssociative clojure.lang.Indexed]
   :instance-fns {assocN [:this :any :any]
                  pop [:this]}}

  clojure.lang.IType
  {:is-interface true}

  clojure.lang.IndexedSeq
  {:is-interface true
   :extends [clojure.lang.ISeq clojure.lang.Sequential clojure.lang.Counted]
   :instance-fns {index [:this]}}

  clojure.lang.Sorted
  {:is-interface true
   :instance-fns {comparator [:this]
                  entryKey [:this :any]
                  seq [:this]
                  seqFrom [:this :any :any]}}}
