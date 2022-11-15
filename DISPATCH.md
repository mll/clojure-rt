# Static / dynamic dispatch algorithm explanation description

## Static calls:

Imagine each var could be created / assigned to once only and this can only be a top level expression.
Imagine also that the function object's type survived (e.g. it was not extracted from polymorphic data structure or any branching - e.g. we analyse here sth like

(defn aa [x] x) 

or 

(def aa (fn [x] x))

but not

(def aa (nth v 3)) ; where we somehow know that 4th element of v is a function

and not 

(def aa (if (= x 5) (fn [x] x) (fn [x] (+ x 1))))


If this is the case dynamic dispatch is enough to guarantee all the methods get optimised. Here is how:

1. The FnNode is encountered somewhere inside the DefNode evaluation. It is being given a unique name of the form "fn_3". Its type is set to function and gets annotated with a constant ConstantFunction and value "fn_3". The FnNode is stored in a global table under this name (TheProgramme->Functions).

2. DefNode encounteres this value with its type intact and uses the constant annotation inside type to get the name of the function. The var name to function name mapping is stored in TheProgramme->StaticFunctions.

3. No function bodies are generated or added to JIT at this stage. 

4. Somewhere later in the programme, we encounter an InvokeNode, which already has knowledge of count and types of arguments to be used. It also has a reference either to var or to fn object being called (and again, we assume its type is intact). In case of var we use TheProgramme->StaticFunctions together with TheProgramme->Functions to extract FnNode corresponding to function, in case of fn, we simply use TheProgramme->Functions to get it (again, we assue fn type is intact so its constant will hold fn name).

At this stage we try to match a FnMethodNode to the arguments.

Knowing the types and fn name we create a qualified name: for example "fn_0_JJD" for a function that accepts two integers and a double. The return value is uniquely determined (in worst case it is "LO" for a boxed any object) so we do not need to store it inside the name. Return value detection algorithm has to take recursive functions into account and is not simple, its description can be found in [TODO reference]. Then, the FnMethodNode will be added to JIT under "fn_0_JJD" for lazy processing and a call to "fn_0_JJD" will be inserted in place of the InvokeNode. This way we only ask JIT to create symbols for arities/methods that potentially can be encountered, but no function is materialised until the actual execution is requested by the running code.

This type of optimisation produces code of speed comparable to pure C. An example here is the naive recursive fibonacci - on my machine 

(defn fib [x] (if (= x 1) 1 (if (= x 2) 1 (+ (fib (- x 1)) (fib (- x 2))))))
(fib 42)

takes ~ 21s for clojure on java, 0.45s for clang C with -Ofast and 0.45s for our static implementation in clojure-rt, where we have added tail call elimination pass to our llvm pipeline.


Clojure is a dynamic language, however - and this dynamicity can destroy our result in at least two ways.

1. The argument types of the function may not be known at compile time. 
2. We may not know the exact type of the object on which we invoke.
3. The var the function was allocated to was modified to contain a different function, this modification possibly occurred on a different thread.

Scenario 3 implies 2, with an additional complication that we already may have calls to our static representation using TheProgramme->StaticFunctions in code already generated. Both 3 and 2 are independent of 1.

We deal with these complexities separately, but in an orthogonal way so that any combination of these occurrences can be handled.

So, here we go:

## Problem 1: The argument types of the function may not be known at compile time. 

We had two choices here. Choice one is trivial - for example we create "fn_0_LOJ" - one generic parameter and one integer as args. We can then compute the return type (which is not necessarily LO, some functions used inside the method might force a return type even if all args are LO) and use the same mechanism that was used for building specialised function but now we do not specify types of some args. 

This has the advantage of delaying unpackaging of arguments until their types are required by the nested function calls (e.g. some functions do not have all the arities because their type would end up undefined, for example there is no "+" function for a string and a list (as "+" is reserved to numerical types in clojure). So a generic implementation of plus, "LOLO", would peek at types of generic arguments in runtime and decide if and how it can proceed or throw a runtime exception expalining that types do not match.

The problem of this approach is - very quickly the dynamic part of the programme can swallow the static one, making all the calls LO.
All the benefits of LLVM optimisations and fast math are then lost.

The other option is to create "fn_0_LOJ" only as a bootstrap. This bootstrap, once run, would determine the exact types of its first generic argument and discover what the actual function to call is, for example if the first argument is actually an integer, it will discover the function is "fn_0_JJ". The interesting part is that this function, which is being run on the "compiled programme" side of compilation can still call runtime library functions that interact directly with the compiler and JIT. So the bootstrap function can ask the jit to create a symbol for "fn_0_JJ" (without materialising it), get a function pointer to this function (which immediately exists for every symbol, JIT uses it as a stub if the function body was not yet materialised), unbox the first argument (since we are now certain it is an integer), use the pointer to call the function and possibly box end result (if generic function type does not correspond to concrete type). This way the price we pay for a dynamic call is confined to type determination (we can use a table for that), calling runtime library to give us function pointer, subsequent call to concrete implementation and boxing possible of the result. 

The signature determination can be hastened by encoding it using the idea that we have at most 256 (one byte) "fast internal types". Therefore, a single 64bit integer can hold at most 8 bytes, so we can service up to 8 function arguments by simple bit shifts. We can use up to 3 integers like this, supporting up to 24 arguments (original clojure supports up to 20). Furthermore, we can cache a few first functions as function pointers attached to the fn structure using atomic CompareAndSwap.  

Cost when not cahced (first run) = type-determination (small, 0 function calls, a GEP and a load) + cost to unbox primitive arguments (cheap, just a GEP and load) + JIT-call (possibly large cost)
 
















