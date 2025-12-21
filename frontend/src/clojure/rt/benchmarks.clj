(ns clojure.rt.benchmarks  
  (:require [criterium.core :as criterium]
            [clojure.test :refer [is deftest]]))

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


(defn vector-assoc-update [size]
  ;; 1. Initialize vector with zeros using transients (matching your C setup)
  (let [initial-v (loop [i 0 
                         v (transient [])]
                    (if (< i size)
                      (recur (inc i) (conj! v 0))
                      (persistent! v)))]
    
    (println "Starting Assoc Update Test...")
    
    ;; 2. Benchmark the assoc loop
    (let [start-time (System/nanoTime)
          
          ;; This 'reduce' acts like your for-loop: v = assoc(v, i, 7)
          final-v (reduce (fn [v i] 
                            (assoc v i 7)) 
                          initial-v 
                          (range size))
          
          end-time (System/nanoTime)
          duration (/ (- end-time start-time) 1e9)]
      
      (printf "Vector Assoc Update Time: %.6f s%n" (double duration))
      
      ;; 3. Check integrity: Sum the results
      ;; reduce is the idiomatic way to iterate/sum in Clojure
      (let [total-sum (reduce + final-v)
            expected-sum (* (long size) 7)]
        
        (if (= total-sum expected-sum)
          (println "Success: Sum is" total-sum)
          (throw (Exception. (str "Wrong result! Expected " expected-sum " but got " total-sum))))))))


;; --- 1. Linear Regression Math ---
(defn calculate-linear-regression [x-vals y-vals]
  (let [n (count x-vals)
        sum-x (reduce + x-vals)
        sum-y (reduce + y-vals)
        sum-xy (reduce + (map * x-vals y-vals))
        sum-x2 (reduce + (map * x-vals x-vals))
        sum-y2 (reduce + (map * y-vals y-vals))
        ;; Formula for slope (b)
        numerator (- (* n sum-xy) (* sum-x sum-y))
        denominator (- (* n sum-x2) (* sum-x sum-x))
        slope (if (zero? denominator) 0 (/ numerator denominator))
        ;; Formula for R^2
        r-num (* numerator numerator)
        r-den (* denominator (- (* n sum-y2) (* sum-y sum-y)))
        r-squared (if (zero? r-den) 0 (/ r-num r-den))]
    {:slope (double slope) :r-squared (double r-squared)}))

;; --- 2. Scaling Test Harness ---
(defn test-scaling-behavior [test-fn]
  (let [sizes [10000 20000 30000 40000 50000 100000 1000000]
        ;; We run a "warm-up" to trigger JIT compilation before measuring
        _ (do (println "Warming up JVM...") (test-fn 50000))
        
        results (for [sz sizes]
                  (let [start (System/nanoTime)
                        _ (test-fn sz)
                        end (System/nanoTime)
                        duration-secs (/ (- end start) 1e9)]
                    (printf "Size %d: %.6f s%n" sz (double duration-secs))
                    {:size sz :time duration-secs}))
        
        x-vals (map :size results)
        y-vals (map :time results)
        {:keys [slope r-squared]} (calculate-linear-regression x-vals y-vals)]

    (printf "%nScaling Analysis:%nSlope: %e s/item%nR^2: %f%n" slope r-squared)

    ;; --- 3. Assertions ---
    (is (> slope 0) "Slope should be positive")
    (is (> r-squared 0.95) "Execution time should scale linearly with size")))



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
