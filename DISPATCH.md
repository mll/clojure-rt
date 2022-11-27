# Static / dynamic dispatch algorithm explanation description


## Notation

Dynamic dispatch:

1. Wszystkie argumenty sa znane na poziomie kompilacji - wywolujemy bezposrednio funkcje,
   sygnatura jest w nazwie. Sygnatura:

Z - boolean, unpacked
C - uft8 char (uint32_t), unpacked
J - int64_t, unpacked
D - double, unpacked
 
Lx - packed value, for example:

LJ int64_t, packed
LC uint32_t, utf8 char, packed
LD double, packed
LZ boolean, packed

Typy ktore zawsze pozostaja spakowane:

LS - String
LV - Vector
LL - List
LM - Map
LA - Array Map   
LY - Symbol
LK - Keyword
LR - Ratio
LH - Big Integer
LN - Nil

LO - Object - moze byc czymkolwiek spakowanym, trzeba dokonac runtime introspection aby sprawdzic

Uwaga! Zalozenie jest takie, ze nigdy nie bedzie typem podawanym do funkcji 
  (niestety tak sie chyba nie da, beda generyczne funkcje...)

LO

Dynamic dispatch powinien to zawsze wyrugowac. Za to jesli chodzi o wartosc zwracana, funkcje moga zwracac object. Przyklad takiej funkcji: 

(defn dyn-ret [x] (if x 1 2.0))

Ta funkcja bedzie zwracala Object z argumentem typu bool i jej dokladna wartosc zwracana nie bedzie znana w trakcie jej kompilacji. Zatem sygnatura bedzie albo:

Z_LO

albo:

LZ_LO

Ale juz w przypadku nil bedzie:

LN_D

Funkcje zawsze preferuja zwracanie typow rozpakowanych, to znaczy ze nie moga zwrocic LD tylko zawsze albo LO albo D.
 

Przyklad zlozonej sygnatury:

ZJD_D

funkcja ktora przyjmuje trzy rozpakowane argumenty - Bool, int, double i produkuje double. 




## Static calls:

Imagine each var could be created / assigned to once only and this can only be a top level expression.
Imagine also that the function object's type survived (e.g. it was not extracted from polymorphic data structure or any branching) - for example:

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

takes ~ 21s for clojure on java, 0.45s for clang C with -Ofast and 0.45s for our implementation in clojure-rt, where we have added tail call elimination pass to the llvm pipeline.

Clojure is a dynamic language, however, and this dynamicity can destroy our result in at least three ways.

I. The argument types of the function may not be known at compile time. 
II. We may not know the exact type of the function object on which we invoke.
III. The var the function was allocated to was modified to contain a different function, this modification possibly occurred on a different thread.

Scenario 3 implies 2, with an additional complication that we already may have calls to our static representation using TheProgramme->StaticFunctions in code already generated. Both 3 and 2 are independent of 1.

We deal with these complexities separately, but in an orthogonal way so that any combination of these occurrences can be handled.

So, here we go:

## Problem I: The argument types of the function may not be known at compile time. 

We had two choices here. Choice one is trivial - for example we create "fn_0_LOJ" - one generic parameter and one integer as args. We can then compute the return type (which is not necessarily LO, some functions used inside the method might force a return type even if all args are LO) and use the same mechanism that was used for building specialised function but now we do not specify types of some args. 

This has the advantage of delaying unpackaging of arguments until their types are required by the nested function calls (e.g. some functions do not have all the arities because their type would end up undefined, for example there is no "+" function for a string and a list (as "+" is reserved to numerical types in clojure). So a generic implementation of plus, "LOLO", would peek at types of generic arguments in runtime and decide if and how it can proceed or throw a runtime exception expalining that types do not match.

There is a problem with this approach - very quickly the dynamic part of the programme can swallow the static one, making all the calls LO.
All the benefits of LLVM optimisations and fast math are then lost.

The other option is to create "fn_0_LOJ" only as a bootstrap. This bootstrap, once run, would determine the exact types of its first generic argument and discover what the actual function to call is. For example, if the first argument is actually an integer, it will discover the function is "fn_0_JJ". The interesting part is that this function, which is being run on the "compiled programme" side of compilation can still call runtime library functions that interact directly with the compiler and JIT. So the bootstrap function can ask the jit to create a symbol for "fn_0_JJ" (without materialising it), get a function pointer to this function (which immediately exists for every symbol, JIT uses it as a stub if the function body was not yet materialised), unbox the first argument (since we are now certain it is an integer), use the pointer to call the function and possibly box end result (if generic function type does not correspond to concrete type). This way, the price we pay for a dynamic call is confined to type determination (we can use a table for that), calling runtime library to give us the function pointer, subsequent call to concrete implementation and finally boxing of the result. 

The signature determination can be hastened by encoding it using the idea that we have at most 256 (one byte) "fast internal types". Therefore, a single 64bit integer can hold at most 8 bytes, so we can service up to 8 function arguments by simple bit shifts. We can use up to 3 integers like this, supporting up to 24 arguments (original clojure supports up to 20). Furthermore, we can cache a few first functions as function pointers attached to the fn structure using atomic CompareAndSwap.  

Cost when not cahced (first run) = type-determination (small, 0 function calls, a GEP and a load) + cost to unbox primitive arguments (cheap, just a GEP and load) + JIT-call (possibly large cost)
 
## Problem II: The exact type of the function object on which we invoke is not known.

This problem arises when we push a function object through a polymorphic data structure. The static type information that allows us to determine association between the function object and function AST representation is then lost.

Example:

(defn ext [x y] ((if (= x 1) (fn [z] (+ 3 z)) (fn [z] (str z "aaa"))) y))

now (ext 1 3) produces 6 and (ext 2 3) produces "3aaa". 

There is no easy way for us to determine which branch would be picked during compilation. 

Furthermore, the types of arguments can be either known or unknown at compile time, but even if known they are of no help whatsoever, since we do not know what function is to be run at runtime. 

The runtime execution has to proceed in a similar way to suggested second solution to problem I - with the improvemnent that argument deduction can be shifted to compilation step and directly injected if all parameter types are known. again - function pointers are being obtained from the runtime and called dynamically. 

## Problem 3: 3. The var the function was allocated to was modified to contain a different function, this modification possibly occurred on a different thread.

This is the culmination of difficulties presented in the document. Not only the arguments can be LO, we do not know the function body at runtime, but also the initial static optimisation is in jeopardy. 

The problem arises when we redefine a function:

(defn fun [x] (+ 3 x))
(defn fun [x] (+ 4 x))

Or, even worse:

(defn ext [x] (if (= x 1) (defn fun [z] (+ z 3)) (defn fun [z q] (+ z q))))

The +3 function will be encountered during program compilation as the first one and it will become our static implementation, but if the second branch of if is chosen we will not benefit from static optimisation at all.

The proposed solution is to add a dynamic check of the corresponding var's content before invokation and if it no longer corresponds to static function - the code would fall back to case II described above.















