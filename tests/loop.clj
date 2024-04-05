(loop [x 2] (if (< 1000 x) x (recur (+ x x))))

(defn abn
  "a * b ^ n"
  [a b n]
  (loop [a a n n]
    (if (= n 0)
      a
      (recur (* a b) (- n 1)))))
(abn 9N 2 1000000)

(defn bounce
  [x n]
  (loop [x x n n]
    (if (= n 0)
      x
      (if (< 1 x)
        (recur (/ x 4) (- n 1))
        (recur (* x 4) (- n 1))))))

(bounce 1/2 1000000)

(defn f [x] (loop [x x] (if (< x 10) x (recur (* x 1/2)))))
(f 99)

(f (+ (abn 9N 2 10000) 8))
(f 512)

(defn easy-bounce
  [x n]
  (loop [x x n n]
    (if (= n 0)
      x
      (if (= 12345 x)
        (recur "sss" (- n 1))
        (recur 12345 (- n 1))))))

(easy-bounce 12345 1000000)

(defn hard-bounce
  [x n]
  (loop [x x n n]
    (if (= n 0)
      x
      (if (= [] x)
        (recur "sss" (- n 1))
        (recur [] (- n 1))))))

(hard-bounce [] 1000000)

(defn map-abn
  "a * b ^ n"
  [a b n]
  (loop [a a n n]
    (if (= (:n n) 0)
      a
      (recur {:a (* (:a a) (:b b))} {:n (- (:n n) 1)}))))
(map-abn {:a 9N} {:b 2} {:n 1000000})
