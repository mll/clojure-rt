# Clojure Real Time

Clojuire Real Time (clojure-rt) is a compiler of Clojure programming language.

It is being developed to allow deterministic and fast execution that could allow Clojure to proliferate beyond its 
original domains of application. It uses LLVM for agressive optimisations, JIT and platform independence.

The compiler strives to be a full implementation of Clojure following the reference java implementation as closely as possible.

The interop part of java has been reimplemented in C to enable the compilation of *.clj part of the clojure source code (this is work in progress).

All the data structures have been developed in C. The compiler uses JIT to compile clojure->llvm->binary at runtime.
The primary advantage of this implementation is reference counting memory model, based on Renking et al, MSR-TR-2020-42, Nov 29, 2020. 
It allows clojure programmes to behave predictably with respect to memory management and execution time, as no garbage collector is involved. 
Furthermore, advanced optimisations allowed by llvm enable the compiler to often execute clojure code at nearly native speeds.

Type annotations, commonly used in the java implementation to speed the execution up are not needed in clojure-rt as 
all the types are being discovered during programme execution and the resulting JITted representations are optimised to benefit from static type
analysis. To archieve the above result, a dynamic dispatch technique, modelled in concept after Julia compiler has been developed.

Currently the compiler is set up in bootstrap mode and consists of two separate parts

1. Compiler frontend written in Clojure itself, to be executed using leiningen / java clojure
   Based on org.clojure/tools.reader and org.clojure/tools.analyzer.
   
   The frontend therefore is very simple, it only adds some additional passes to compute memory management annotations.

2. Compiler backend written in C++ (llvm part) and C (runtime + basic standard library).
   
   The backend is composed of llvm code generator, llvm JIT infrastructure and bootstraps and implementation of clojure runtime in C (for performance).
   All the basic immutable data structures are implemented in C, allowing the highest level of optimisation.

The parts of the compiler currently communicate using protocol buffers (the *.cljb files). However, as soon as clojure itself will be able to be compiled 
using the two-part bootstrap compiler, a self-hosted compiler will be developed.

## Compilation

The compilation process is tested on OS X only, but due to cmake it should work everywhere if you manage to install dependencies from begin_development 
using your system's package manager.

1. ./begin_development.sh
2. cd backend
3. cmake . -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_BUILD_TYPE=Debug ;; or Release
4. make -j 8

This should build the c/c++ compiler backend.

## Running

./compile.sh fib.clj

Please note that currently the backend prints out a lot of debug information. The primary info it prints is the LLVM code generated for given clojure statements.
Please also note, that currently the programmes compiled bu the frontend have to be *executed* by the frontend to generate the AST. Therefore, running 
the naive recursive (fib 42) runs on my machine for 26 seconds (frontend using java implementation) and 0.46 seconds (backend, clojre-rt). 
This not only shows how much faster clojure-rt can be, but also how much will be gained when the compiler bootstrapping process will be complete.

## Generating protobuf models

The models do not need to be regenerated before compilation. However, there might be a time in development when such generation can become necessary.
The process is described below.

The original source of protobuf models come from org.clojure/tools.analyzer.jvm documentation of its AST representation.

We use two files to describe the AST as a data structure: 

frontend/resources/ast-types.edn 
frontend/resources/ast-ref.edn

These ast-ref.edn can be readily obtained from the analyzer repository, they can also be autogenerated using a script found in this repository. 
The types file is manually created to assign protobuf types to elements of AST tree produced by the analyser (needed here as clojure is dynamically typed).

The frontend application is not only capable of compiling .clj into .cljb (protobuf representation of AST) but also can be used to generate the protobuf definition
(model/bytecode.proto) from the above files.

Then, protoc can be used to generate clojure and c++ specific code for decoding / encoding .cljb

TL;DR: The whole process is automated and can be run this way:

cd model
./generate.sh

## self-hosted compiler roadmap

The self-hosted compiler will consist of:

A. Compiler backend, exactly as (2) above.
B. Compiled protobuf file of clojure itself. It will be generated from the main clojure repository using the bootstrap compiler (1)
C. Compiled protobuf files of (1) with all its dependencies.
D. Bootstrapper as main function of the C++ application:

The bootstrapper would launch the backend with pre-build protobuf files as initial input. Then, it will compile the frontend 
(later we could use the LLVM representation of the complete compilation result so that backend would start more rapidly).

Finally, any clojure files to be compiled and run (including possible REPL commands) would be fed to the running system, first passed through the frontend and finally 
to the backend for execution. The protobuf files will probably be transferred between frontend and backend as in-memory data structures at this stage, as both 
parts of the compiler would reside inside the same process.


## Static / dynamic dispatch algorithm explanation description
### Notation

Dynamic dispatch:

1. All arguments are known at the compilation level - we call functions directly, signature is included in function name. 

Signature explanation:

Z - boolean, unpacked
C - uft8 char (uint32_t), unpacked
J - int64_t, unpacked
D - double, unpacked
 
Lx - packed value, for example:

LJ int64_t, packed
LC uint32_t, utf8 char, packed
LD double, packed
LZ boolean, packed

Types that always remain packed:

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

LO - Object - can be anything that is packaged. Runtime inspection is needed to determine the exact type.

Dynamic dispatch eliminates LO from function arguments. Type guessing and conditional compilation allows for eliminating it further from 
the results of function calls within functions. However, it can still remain as a return value. 

Example:

(defn dyn-ret [x] (if x 1 2.0))

If it gets a boolean argument, its return value will remain unknown during compilation. The signature will be:

Z_LO

or:

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

2. DefNode encounteres this value with its type intact and uses the constant annotation inside type to get the name of the function.

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

### Problem I: The argument types of the function may not be known at compile time. 

We had two choices here. Choice one is trivial - for example we create "fn_0_LOJ" - one generic parameter and one integer as args. We can then compute the return type (which is not necessarily LO, some functions used inside the method might force a return type even if all args are LO) and use the same mechanism that was used for building specialised function but now we do not specify types of some args. 

This has the advantage of delaying unpackaging of arguments until their types are required by the nested function calls (e.g. some functions do not have all the arities because their type would end up undefined, for example there is no "+" function for a string and a list (as "+" is reserved to numerical types in clojure). So a generic implementation of plus, "LOLO", would peek at types of generic arguments in runtime and decide if and how it can proceed or throw a runtime exception expalining that types do not match.

There is a problem with this approach - very quickly the dynamic part of the programme can swallow the static one, making all the calls LO.
All the benefits of LLVM optimisations and fast math are then lost.

The other option is to create "fn_0_LOJ" only as a bootstrap. This bootstrap, once run, would determine the exact types of its first generic argument and discover what the actual function to call is. For example, if the first argument is actually an integer, it will discover the function is "fn_0_JJ". The interesting part is that this function, which is being run on the "compiled programme" side of compilation can still call runtime library functions that interact directly with the compiler and JIT. So the bootstrap function can ask the jit to create a symbol for "fn_0_JJ" (without materialising it), get a function pointer to this function (which immediately exists for every symbol, JIT uses it as a stub if the function body was not yet materialised), unbox the first argument (since we are now certain it is an integer), use the pointer to call the function and possibly box end result (if generic function type does not correspond to concrete type). This way, the price we pay for a dynamic call is confined to type determination (we can use a table for that), calling runtime library to give us the function pointer, subsequent call to concrete implementation and finally boxing of the result. 

The signature determination can be hastened by encoding it using the idea that we have at most 256 (one byte) "fast internal types". Therefore, a single 64bit integer can hold at most 8 bytes, so we can service up to 8 function arguments by simple bit shifts. We can use up to 3 integers like this, supporting up to 24 arguments (original clojure supports up to 20). Furthermore, we can cache a few first functions as function pointers attached to the fn structure using atomic CompareAndSwap.  

Cost when not cahced (first run) = type-determination (small, 0 function calls, a GEP and a load) + cost to unbox primitive arguments (cheap, just a GEP and load) + JIT-call (possibly large cost)
 
### Problem II: The exact type of the function object on which we invoke is not known.

This problem arises when we push a function object through a polymorphic data structure. The static type information that allows us to determine association between the function object and function AST representation is then lost.

Example:

(defn ext [x y] ((if (= x 1) (fn [z] (+ 3 z)) (fn [z] (str z "aaa"))) y))

now (ext 1 3) produces 6 and (ext 2 3) produces "3aaa". 

There is no easy way for us to determine which branch would be picked during compilation. 

Furthermore, the types of arguments can be either known or unknown at compile time, but even if known they are of no help whatsoever, since we do not know what function is to be run at runtime. 

The runtime execution has to proceed in a similar way to suggested second solution to problem I - with the improvemnent that argument deduction can be shifted to compilation step and directly injected if all parameter types are known. again - function pointers are being obtained from the runtime and called dynamically. 

### Problem III: The var the function was allocated to was modified to contain a different function, this modification possibly occurred on a different thread.

This is the culmination of difficulties presented in the document. Not only the arguments can be LO, we do not know the function body at runtime, but also the initial static optimisation is in jeopardy. 

The problem arises when we redefine a function:

(defn fun [x] (+ 3 x))
(defn fun [x] (+ 4 x))

Or, even worse:

(defn ext [x] (if (= x 1) (defn fun [z] (+ z 3)) (defn fun [z q] (+ z q))))

The +3 function will be encountered during program compilation as the first one and it will become our static implementation, but if the second branch of if is chosen we will not benefit from static optimisation at all.

The proposed solution is to add a dynamic check of the corresponding var's content before invokation and if it no longer corresponds to static function - the code would fall back to case II described above.

## License

clojure-rt is being distributed on GPLv2 license. See LICENSE.md for details.

Copyright © 2022-2024 Marek Lipert, Aleksander Wiacek
















