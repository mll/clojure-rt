(ns tmp.core
  (:require [clojure.tools.reader :as r]
            [clojure.tools.analyzer.jvm :as a]
            [clojure.tools.emitter.jvm :as e]))

(defn foo
  "I don't do a whole lot."
  [x]
  (println x "Hello, World!"))

(defn boo [a] (println "aa"))
