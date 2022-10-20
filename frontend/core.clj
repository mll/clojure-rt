(ns clojure.rt.core
  (:require [clojure.tools.reader :as r]
            [clojure.tools.analyzer.jvm :as a]
            [clojure.tools.emitter.jvm :as e]
            [clojure.rt.protobuf.schema :as sch]
            [clojure.rt.protobuf.encoder :as enc]
            [protojure.protobuf :as protojure]
            [clojure.string :refer [join split]]
            [clojure.tools.reader.reader-types :as t]))

(defn analyze [s] 
  (let [reader (t/source-logging-push-back-reader s)]
    (loop [form (r/read {:eof :eof} reader) ret-val []]
      (if (= :eof form) ret-val
          (do
            (eval form)
            (recur (r/read {:eof :eof} reader)
                   (conj ret-val (a/analyze form))))))))


(defn generate-protobuf-defs [] (sch/generate-protobuf-defs "bytecode.proto"))

;; (compile "(+ 3 2)" "bytecode.clb")
(defn compile [form outfile]
  (clojure.java.io/copy
   (protojure/->pb (enc/encode-ast (analyze form)))
   (java.io.File. outfile)))

(defn -main 
  ([infile] (let [parts (split infile #"\.")] 
              (compile (slurp infile) (str (join "." (butlast parts)) ".cljb"))))
  ([] (println "Generating protobuf definitions into bytecode.proto file. To compile use file name as parameter")
   (generate-protobuf-defs)))
