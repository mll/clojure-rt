[(+ 2.5 2.5)
 (+ 2.5 1/2)
 (+ 2.5 9N)
 (+ 2.5 4)
 
 (+ 1/2 2.5)
 (+ 1/2 9N)
 (+ 1/2 4)
 (+ 1/2 1/2)
 
 (+ 9N 2.5)
 (+ 9N 1/2)
 (+ 9N 9N)
 (+ 9N 4)
 
 (+ 4 2.5)
 (+ 4 1/2)
 (+ 4 9N)
 (+ 4 4)]

(defn huge [n x]
  (if (= n 0)
    x
    (recur (- n 1) (* x x))))

(huge 9 0.99)
(huge 8 10N) ;; 1e256, fits in double
(huge 9 10N) ;; 1e512, doesn't fit in double
(huge 9 99/100)
(huge 9 (/ 99 100))

(- 2 (/ 1 3))

(java.lang.Math/sin 256)
(java.lang.Math/sin (huge 3 2))
(java.lang.Math/sin 10N)
(java.lang.Math/sin (huge 8 10N))
(java.lang.Math/sin (huge 9 10N))
(java.lang.Math/sin 99/100)
(java.lang.Math/sin (/ 99 100))
(java.lang.Math/sin (huge 9 99/100))
(java.lang.Math/sin (huge 9 (/ 99 100)))

[(clojure.lang.Numbers/gte 2.5 2.5)
 (clojure.lang.Numbers/gte 2.5 1/2)
 (clojure.lang.Numbers/gte 2.5 9N)
 (clojure.lang.Numbers/gte 2.5 4)
 
 (clojure.lang.Numbers/gte 1/2 2.5)
 (clojure.lang.Numbers/gte 1/2 1/2)
 (clojure.lang.Numbers/gte 1/2 9N)
 (clojure.lang.Numbers/gte 1/2 4)
 
 (clojure.lang.Numbers/gte 9N 2.5)
 (clojure.lang.Numbers/gte 9N 1/2)
 (clojure.lang.Numbers/gte 9N 9N)
 (clojure.lang.Numbers/gte 9N 4)
 
 (clojure.lang.Numbers/gte 4 2.5)
 (clojure.lang.Numbers/gte 4 1/2)
 (clojure.lang.Numbers/gte 4 9N)
 (clojure.lang.Numbers/gte 4 4)]

[(clojure.lang.Numbers/lt 2.5 2.5)
 (clojure.lang.Numbers/lt 2.5 1/2)
 (clojure.lang.Numbers/lt 2.5 9N)
 (clojure.lang.Numbers/lt 2.5 4)
 
 (clojure.lang.Numbers/lt 1/2 2.5)
 (clojure.lang.Numbers/lt 1/2 1/2)
 (clojure.lang.Numbers/lt 1/2 9N)
 (clojure.lang.Numbers/lt 1/2 4)
 
 (clojure.lang.Numbers/lt 9N 2.5)
 (clojure.lang.Numbers/lt 9N 1/2)
 (clojure.lang.Numbers/lt 9N 9N)
 (clojure.lang.Numbers/lt 9N 4)
 
 (clojure.lang.Numbers/lt 4 2.5)
 (clojure.lang.Numbers/lt 4 1/2)
 (clojure.lang.Numbers/lt 4 9N)
 (clojure.lang.Numbers/lt 4 4)]
