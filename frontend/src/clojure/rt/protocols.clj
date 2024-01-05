(ns clojure.rt.protocols
  (:refer-clojure :exclude [compile])
  (:require [clojure.string :refer [join split]]
            [clojure.tools.reader.reader-types :as t]))

;; clojure.lang 

(defprotocol IAtom
  (swap 
    [this f] 
    [this f arg]
    [this f arg1 arg2]
    [this f x y args])
  (compareAndSwap [this oldv newv])
  (reset [this newval]))

; Atom2 extends Atom

(defprotocol IAtom2
  (swapVals 
    [this f]
    [this f arg]
    [this f arg1 arg2]
    [this f x y seq])
  (resetVals [this newv]))

(defprotocol IBlockingDeref 
  (deref [this ms timeoutValue]))

(defprotocol Counted 
  (count [this]))

; Indexed extends Counted

(defprotocol Indexed
  (nth [i])
  (nth [i notFound]))

; IChunk extends Indexed

(defprotocol IChunk
  (dropFirst [this])
  (reduce [f start]))

(defprotocol Sequable
  (seq [this]))

; IPersistentCollection extends Sequable

(defprotocol IPersistentCollection
  (count [this])
  (cons [this o])
  (empty [this])
  (equiv [this o]))

; ISeq extends IPersistentCollection

(defprotocol ISeq
  (first [this])
  (next [this])
  (more [this])
  (cons [this o]))

(defprotocol IDeref
  (deref [this]))

(defprotocol Sequential)

; IDrop extends Sequential
(defprotocol IDrop
  (drop [this n]))

(defprotocol IExceptionInfo
  (getData [this]))

; java.util.concurrent

(defprotocol Callable
  (call [this]))

; java.lang

(defprotocol Runnable
  (run [this]))

; IFN extends Callable and Runnable

(defprotocol IFn
  (invoke 
    [this] 
    [this arg1] 
    [this arg1 arg2]
    [this arg1 arg2 arg3]
    [this arg1 arg2 arg3 arg4]
    [this arg1 arg2 arg3 arg4 arg5]
    [this arg1 arg2 arg3 arg4 arg5 arg6]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12 arg13]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12 arg13 arg14]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12 arg13 arg14 arg15]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12 arg13 arg14 arg15 arg16]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12 arg13 arg14 arg15 arg16 arg17]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12 arg13 arg14 arg15 arg16 arg17 arg18]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12 arg13 arg14 arg15 arg16 arg17 arg18 arg19]
    [this arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9 arg10 arg11 arg12 arg13 arg14 arg15 arg16 arg17 arg18 arg19 arg20])
  ; variadic?
  (applyTo [this arglist]))

(defprotocol IHashEq
  (hasheq [this]))

(defprotocol IKVReduce
  (kvreduce [this f init]))

(defprotocol IKeywordLookup
  (getLookupThunk [this k]))

(defprotocol ILookup
  (valAt [this key])
  (valAt [this ket notFound]))

(defprotocol ILookupSite 
  (fault [this target]))

(defprotocol ILookupThunk
  (get [this target]))

; java.util

(defprotocol Iterator 
  (forEachRemaining [this action])
  (hasNext [this])
  (next [this])
  (remove [this]))

; extends java.util.Map.Entry

(defprotocol IMapEntry
  (key [this])
  (val [this]))

(defprotocol IEditableCollection
  (asTransient [this]))

(defprotocol IMeta 
  (meta [this]))

; IObj extends IMeta

(defprotocol IObj
  (withMeta [meta]))

(defprotocol IPending
  (isRealized [this]))

; IPersistentStack extends IPersistentCollection

(defprotocol IPersistentStack
  (peek [this])
  (pop [this]))

; IPersistentList extends Sequential, IPersistentStack

(defprotocol IPersistentList)
  

; Associative extends IPersistentCollection, ILookup  
(defprotocol Associative
  (containsKey [this key])
  (entryAt [this key])
  (assoc [this key val]))

(defprotocol Iterable
  (iterator [this])
  (forEach [this action])
  (spliterator [this]))

; IPersistentMap extends Iterable, Associative, Counted
(defprotocol IPersistentMap
  (assoc [this key val])
  (assocEx [this key val])
  (without [this key]))

; IPersistentSet extends IPersistentCollection, Counted
(defprotocol IPersistentSet
  (disjoin [this key])
  (contains [this key])
  (get [this key]))

(defprotocol Reversible 
  (rseq [this]))

; IPersistentVector extends Associative Sequential IPersistentStack Reversible Indexed
(defprotocol IPersistentVector
  (assocN [this i val])
  (cons [this o]))

(defprotocol IProxy
  (__initClojureFnMappings [this m])
  (__updateClojureFnMappings [this m])
  (__getClojureFnMappings [this]))

(defprotocol IRecord)

(defprotocol IReduceInit
  (reduce [this f start]))

; IReduce extends IReduceInit
(defprotocol IReduce
  (reduce [this f]))

(defprotocol IRef
  (setValidator [this vf])
  (getValidator [this])
  (getWatchers [this])
  (addWatch [key callback])
  (removeWatch [key]))

(defprotocol IMeta
  (meta [this]))

; IReference extends IMeta
(defprotocol IReference
  (alterMeta [this alter args])
  (resetMeta [this m]))

(defprotocol ITransientCollection
  (conj [this val])
  (persistent [this]))

; ITransientAssociative extends ITransientCollection, ILookup
(defprotocol ITransientAssociative
  (assoc [this key val]))

; ITransientAssociative2 extends ITransientAssociative
(defprotocol ITransientAssociative2
  (containsKey [this key])
  (entryAt [this key]))

; ITransientMap extends ITransientAssociative, Counted 
(defprotocol ITransientMap 
  (assoc [this key val])
  (without [this key])
  (persistent [this]))

; ITransientSet extends ITransientCollection, Counted
(defprotocol ITransientSet
  (disjoin [this key])
  (contains [this key])
  (get [this key]))

; ITransientVector extends ITransientAssociative, Indexed
(defprotocol ITransientVector
  (assocN [this i val])
  (pop [this]))

(defprotocol IType)

; IndexedSeq extends ISeq, Sequential, Counted
(defprotocol IndexedSeq
  (index [this]))

(defprotocol Sorted
  (comparator [this])
  (entryKey [this entry])
  (seq [this])
  (seqFrom [this key ascending)))
