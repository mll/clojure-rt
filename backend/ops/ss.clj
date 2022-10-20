(println 
 (let [fs (-> "xx" (slurp) (clojure.string/split #"\n"))]
   (clojure.string/join "\n" 
                        (map (fn [f] 
                               (let [[name extension] (clojure.string/split f #"\.")]
                                 (str name "." extension ": ops/" name ".cpp codegen.h\n"
                                      "\t$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/" name ".cpp"))
                               ) fs))))
