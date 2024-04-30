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
             [analyze-host-expr :refer [analyze-host-expr]]
             [box :refer [box]]
             [constant-lifter :refer [constant-lift]]
             [classify-invoke :refer [classify-invoke]]
             [validate :refer [validate]]
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

(def passes-opts
  ":passes-opts for `analyze`"
  {:collect/what                    #{:constants :callsites}
   :collect/where                   #{:deftype :reify :fn :fn-method}
   :collect/top-level?              false
   :collect-closed-overs/where      #{:deftype :reify :fn-method :loop :try}
   :collect-closed-overs/top-level? false})

(defn analyze [s filename]
  (with-bindings {#'a/run-passes run-passes}
    (with-redefs [ana/parse-quote quote/parse-quote]
      (let [reader (t/source-logging-push-back-reader s 1 filename)]
        (loop [form (r/read {:eof :eof} reader) ret-val []]
          (if (= :eof form) (do #_(clojure.pprint/pprint (passes/clean-tree ret-val)) ;; uncomment to see simple tree
                                ret-val)
              (do
                ;; (eval form)
                (recur (r/read {:eof :eof} reader)
                     (->> 
                      (a/analyze form (a/empty-env) {:passes-opts passes-opts})      
                      (conj ret-val))))))))))


(defn generate-protobuf-defs [] (sch/generate-protobuf-defs "bytecode.proto"))

;; (compile "(+ 3 2)" "bytecode.clb")
(defn compile [form outfile infile-name]
  (clojure.java.io/copy
   (protojure/->pb (enc/encode-ast (analyze form infile-name)))
   (java.io.File. outfile)))

(defn -main
  ([infile] (let [parts (split infile #"\.")]
              (compile (slurp infile) (str (join "." (butlast parts)) ".cljb") infile)))
  ([] (println "Generating protobuf definitions into bytecode.proto file. To compile use file name as parameter")
   (generate-protobuf-defs)))


