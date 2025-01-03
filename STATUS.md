alhambra.clj - not implemented yet
arith.clj - works 
class_hash_field_access.clj - doesnt compile
deftype.clj - does not compile
general.clj - does not work! - reason known - to little arguments (dynamic vararg needed)
histogram.clj - does not compile
lambda-capture.clj - does not run - broken function return value (broken function detected by LLVM)
leftn.clj - not implemented yet
like-atom.clj - does not compile
loop-case.clj - works, but case not implemented 
over9000.clj - works
pendulum.clj - does not compile
protocol.clj - does not compile
recur.clj - works, but terminates with an error
reify.clj - does not compile
stalin.clj - works, but really slow (was never able to wait it out)
static-fields.clj - does not work! - heap-use-after-free - problemy z kontrola pamieci w wywolaniach
try.clj - not implemented yet
variadic.clj - does not run - broken function return value
vecpop.clj - works
vectransient.clj - cannot create function (broken function detected by LLVM)
wrong-args.clj - seems to be working now, needs confirmation on intel