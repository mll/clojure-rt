[(+ 2.5 2.5)
 (+ 2.5 9N)
 (+ 2.5 4)
 (+ 9N 2.5)
 (+ 9N 9N)
 (+ 9N 4)
 (+ 4 2.5)
 (+ 4 9N)
 (+ 4 4)]

(defn huge [n x]
  (if (= n 0)
    x
    (recur (- n 1) (* x x))))

(huge 8 10N) ;; 1e256, fits in double
(huge 9 10N) ;; 1e512, doesn't fit in double

(java.lang.Math/sin (huge 8 10N)) 
(java.lang.Math/sin (huge 9 10N)) 

[(clojure.lang.Numbers/gte 2.5 2.5)
 (clojure.lang.Numbers/gte 2.5 9N)
 (clojure.lang.Numbers/gte 2.5 4)
 (clojure.lang.Numbers/gte 9N 2.5)
 (clojure.lang.Numbers/gte 9N 9N)
 (clojure.lang.Numbers/gte 9N 4)
 (clojure.lang.Numbers/gte 4 2.5)
 (clojure.lang.Numbers/gte 4 9N)
 (clojure.lang.Numbers/gte 4 4)]
[(clojure.lang.Numbers/lt 2.5 2.5)
 (clojure.lang.Numbers/lt 2.5 9N)
 (clojure.lang.Numbers/lt 2.5 4)
 (clojure.lang.Numbers/lt 9N 2.5)
 (clojure.lang.Numbers/lt 9N 9N)
 (clojure.lang.Numbers/lt 9N 4)
 (clojure.lang.Numbers/lt 4 2.5)
 (clojure.lang.Numbers/lt 4 9N)
 (clojure.lang.Numbers/lt 4 4)]
