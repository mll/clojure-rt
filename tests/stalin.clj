(defn average [x y]
  (/ (+ x y) 2))

(defn improve [guess x]
  (average guess (/ x guess)))

(defn good-enough? [guess x]
  (< (Math/abs (- (* guess guess) x)) 0.001))

(defn sqrt-iter [guess x]
  (if (good-enough? guess x)
      guess
      (recur (improve guess x)
                 x)))

(defn mysqrt [x]
  (sqrt-iter 1.0 x))

(defn int [x acc step]
  (if (>= x 10000.0)
      acc
      (recur (+ x step)
           (+ acc (* step (mysqrt x)))
           step)))
           
(int 0.0 0.0 0.001)