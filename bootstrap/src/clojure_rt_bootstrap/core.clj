(ns clojure-rt-bootstrap.core
  (:require [clojure.tools.reader :as r]
            [clojure.tools.analyzer.jvm :as a]
            [clojure.tools.emitter.jvm :as e]
            [clojure-rt-bootstrap.protobuf :as pb]
            [protobuf.core :as pbc]))

(defn ana [s] (a/analyze (r/read-string s)))

(defn generate-protobuf-defs [] (pb/generate-protobuf-defs "../model/bytecode.proto"))

;; (compile "(+ 3 2)" "bytecode.clb")
(defn compile [form outfile]
  (pbc/write (pb/encode-ast (ana form)) outfile))
