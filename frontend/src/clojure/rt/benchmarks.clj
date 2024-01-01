(ns clojure.rt.benchmarks  
  (:require [criterium.core :as criterium]))

;; https://clojure-goes-fast.com

;(set! *unchecked-math* true)
(set! *warn-on-reflection* true)

(defn vrange [n]
  (loop [i 0 v []]
    (if (< i n)
      (recur (inc i) (conj v i))
      v)))

(defn vrange2 [n]
  (loop [i 0 v (transient [])]
    (if (< i n)
      (recur (inc i) (conj! v i))
      (persistent! v))))

(defn vrange3 [n]
  (let [array (int-array n)]
    (loop [i 0]
      (if (< i n)
        (do
          (aset array i i)
          (recur (inc i)))
        ))
    array))

(defn vrange4 [n]
  (let [array (java.util.ArrayList.)]
    (loop [i 0]
      (if (< i n)
        (do 
          (.add array i)
          (recur (inc i)))
        ))
    array))


;; benchmarked (Java 1.8, Clojure 1.7)
;; (println "Clojure")
;; (def v (criterium/quick-bench (vrange 10000000)))    ;; ~200 ms
;; (def v nil)
;; (println "Transient")
;; (def v2 (criterium/quick-bench (vrange2 10000000)))  ;; ~170 ms
;; (def v2 nil)
;; (println "aset")
;; (def v3 (criterium/quick-bench (vrange3 10000000)))  ;; 17 ms
;; (def v3 nil)
;; (println "ArrayList")
;; (def v4 (criterium/quick-bench (vrange4 10000000)))  ;; 177 ms
;; (def v4 nil)
