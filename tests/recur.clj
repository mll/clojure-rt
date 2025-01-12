(defn f- [x] (if (< x 10) x (recur (- x 5))))
(f- 99)

(defn abn
  "a * b ^ n"
  [a b n]
  (if (= n 0)
    a
    (recur (* a b) b (- n 1))))
(abn 9N 2 1000000)

(defn bounce
  [x n]
  (if (= n 0)
    x
    (if (< 1 x)
      (recur (/ x 4) (- n 1))
      (recur (* x 4) (- n 1)))))

(bounce 1/2 1000000)

(defn f [x] (if (< x 10) x (recur (* x 1/2))))
(f 99)

(f (+ (abn 9N 2 10000) 8))
(f 512)

;; vararg-call and vararg-recur should return different values

(defn vararg-call
  [n x & args]
  (if (= 0 n)
    [x args]
    (vararg-call (- n 1) (* x x) 2)))

(vararg-call 3 2N :a :b :c)

(defn vararg-recur
  [n x & args]
  (if (= 0 n)
    [x args]
    (recur (- n 1) (* x x) 2)))

(vararg-recur 3 2N :a :b :c)

(defn map-abn
  "a * b ^ n"
  [a b n]
  (if (= (:n n) 0)
    a
    (recur {:a (* (:a a) (:b b))} b {:n (- (:n n) 1)})))
(map-abn {:a 9N} {:b 2} {:n 1000000})

