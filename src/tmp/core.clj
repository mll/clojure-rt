(ns tmp.core
  (:require [clojure.tools.reader :as r]
            [clojure.tools.analyzer.jvm :as a]
            [clojure.tools.emitter.jvm :as e]))




(defn ana [s] (a/analyze (r/read-string s)))
