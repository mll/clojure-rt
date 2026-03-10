(ns clojure.rt.core
  (:refer-clojure :exclude [compile])
  (:require [clojure.tools.reader :as r]
            [clojure.tools.analyzer.jvm :as a]
            [clojure.tools.emitter.jvm :as e]
            [clojure.rt.protobuf.schema :as sch]
            [clojure.rt.protobuf.encoder :as enc]
            [protojure.protobuf :as protojure]
            [clojure.string :refer [join split]]
            [clojure.tools.reader.reader-types :as t]
            [clojure.rt.passes :as passes]
            [clojure.pprint :refer [pprint]]
            [clojure.rt.deftype :as deftype]
            [clojure.rt.quote :as quote]
            [clojure.rt.closed-overs :refer [collect-closed-overs]]
            [clojure.tools.analyzer :as ana]
            [clojure.tools.analyzer
             [utils :refer [ctx resolve-sym -source-info resolve-ns obj? dissoc-env butlast+last mmerge]]
             [ast :refer [walk prewalk postwalk] :as ast]
             [env :as env :refer [*env*]]
             [passes :refer [schedule]]]

            [clojure.tools.analyzer.jvm.utils :refer :all :as u :exclude [box specials]]

            [clojure.tools.analyzer.passes
             [source-info :refer [source-info]]
             [trim :refer [trim]]
             [elide-meta :refer [elide-meta elides]]
             [warn-earmuff :refer [warn-earmuff]]
             [uniquify :refer [uniquify-locals]]]

            [clojure.tools.analyzer.passes.jvm
             [annotate-host-info :refer [annotate-host-info]]
             [analyze-host-expr :refer [analyze-host-expr]]
             [box :refer [box]]
             [constant-lifter :refer [constant-lift]]
             [classify-invoke :refer [classify-invoke]]
             [validate :refer [validate validate-call validate-interfaces]]
             [infer-tag :refer [infer-tag]]
             [validate-loop-locals :refer [validate-loop-locals]]
             [warn-on-reflection :refer [warn-on-reflection]]
             [emit-form :refer [emit-form]]]))


(def rt-passes
  "Set of passes that will be run on the AST by #'run-passes"
  #{#'warn-on-reflection
    #'warn-earmuff

    #'uniquify-locals

    #'source-info
    #'elide-meta
   ;; we do not want constant lifting as it is not compatible with protobuf definitions
   ;; #'constant-lift

    #'trim

    #'box

    #'analyze-host-expr
    #'validate-loop-locals
    #'validate
    #'infer-tag

    #'classify-invoke
    #'collect-closed-overs
    #'passes/simplify-closed-overs
    #'passes/remove-env
    #'passes/fresh-vars
    #'passes/memory-management-pass
    #'passes/rewrite-loops
    })

(def run-passes
  ((fn [] 
     ;; Uncomment to see the sequence of scheduled passes 
     ;(pprint (schedule rt-passes {:debug? true}))
     (schedule rt-passes))))

(defn print-readable-tree [node depth]
  (if (map? node)
    (let [pre (join (repeat depth ">"))
          post (join (repeat depth "<"))]
      (println (str pre " Op​ " (if (= (:op node) :const)
                                  (str "Const type​ " (:type node))
                                  (:op node))))
      (when-not (= (:op node) :map)
        (println (str pre " Form​ "  (with-out-str (pprint (:form node))))))
      (println (str pre " Drop memory: " (:drop-memory node)))
      (println (str pre " Unwind memory: " (:unwind-memory node)))
      (doseq [child (:children node)]
        (println (str pre "! Child: " child))
        (print-readable-tree (child node) (inc depth))
        (println (str post "! Child: " child)))
      (println (str post "|||| Op​ " (if (= (:op node) :const)
                                       (str "Const type​ " (:type node))
                                       (:op node)))))
    (doseq [subnode node]
      (print-readable-tree subnode depth))))


(def passes-opts
  ":passes-opts for `analyze`"
  {:collect/what                    #{:constants :callsites}
   :collect/where                   #{:deftype :reify :fn :fn-method}
   :collect/top-level?              false
   :collect-closed-overs/where      #{:deftype :reify :fn-method :loop :try}
   :collect-closed-overs/top-level? false
   :validate/wrong-tag-handler      (fn [_ _] nil)
   :validate/unresolvable-symbol-handler (fn [_ _ _] nil)})

(defn analyze
  ([s filename] (analyze s filename false false))
  ([s filename trivial-tree? simple-tree?]
   (with-bindings {#'a/run-passes run-passes}
     (with-redefs [ana/parse-quote quote/parse-quote
                   a/parse-deftype* deftype/parse-deftype*
                   annotate-host-info deftype/annotate-host-info
                   validate-call (fn [ast] ast)
                   validate-interfaces (fn [_])]
       (let [reader (t/source-logging-push-back-reader s 1 filename)]
         (loop [form (r/read {:eof :eof} reader) 
                current-env (a/empty-env) 
                ret-val []]
           (if (= :eof form)
             (do
               (when trivial-tree? (print-readable-tree ret-val 1))
               (when simple-tree? (clojure.pprint/pprint ret-val))
               ret-val)
             (let [;; 1. Analyze the form with the accumulated environment
                   ast (a/analyze form current-env {:passes-opts passes-opts})]
               
               ;; 2. Selective Eval for Macros
               ;; If the AST represents a macro definition, we MUST eval it.
               ;; Without this, the next form cannot be macro-expanded.
               (when (or (and (= (:op ast) :def) (:macro (:var ast)))
                         (and (= (:op ast) :do) ; Handle (do (defmacro ...))
                              (some #(and (= (:op %) :def) (:macro (:var %))) (:statements ast))))
                 (eval form))

               (recur (r/read {:eof :eof} reader)
                      ;; 3. Thread the updated environment to the next iteration
                      (:env ast) 
                      (conj ret-val ast))))))))))

(defn generate-protobuf-defs [] (sch/generate-protobuf-defs "bytecode.proto"))

;; (compile "(+ 3 2)" "bytecode.clb")
(defn compile
  ([form outfile infile-name] (compile form outfile infile-name false false))
  ([form outfile infile-name trivial-tree? simple-tree?]
   (-> form
       (analyze infile-name trivial-tree? simple-tree?)
       (enc/encode-ast)
       (protojure/->pb)
       (clojure.java.io/copy (java.io.File. outfile)))))

(defn -main
  ([infile] (let [parts (split infile #"\.")]
              (compile (slurp "resources/rt-protocols.edn") "../backend-v2/rt-protocols.cljb" infile)
              (compile (slurp "resources/rt-classes.edn") "../backend-v2/rt-classes.cljb" infile)
              (compile (slurp infile) (str (join "." (butlast parts)) ".cljb") infile
                       false false)
              ))
  ([] (println "Generating protobuf definitions into bytecode.proto file. To compile use file name as parameter")
      (generate-protobuf-defs)))


