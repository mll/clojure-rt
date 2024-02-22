(defn stick [n x y]
  (if (= n 0)
    (+ x y)
    (let [v0 (stick (- n 1) x y)
          v1 (stick (- n 1) v0 y)]
      v1)))

(stick 25 0N 1N)

(stick 25 0 1)


(defn stick2 [n y]
  (if (= n 0)
    y
    (let [v0 (stick2 (- n 1) y)
          v1 (stick2 (- n 1) y)]
      (+ v0 v1))))

(stick2 25 1N)

(stick2 25 1)


(defn fib-acc
  [n x y]
  (if (= n 0)
    x
    (if (= n 100)
      12345678890N
      (fib-acc (- n 1) y (+ x y)))))

(+ (fib-acc 42 0N 1N) 3N)
(fib-acc 142 0N 1N)


(defn fib-acc-2
  [n x y]
  (if (= n 0)
    x
    (if (= n 100)
      "kuku"
      (fib-acc-2 (- n 1) y (+ x y)))))

(+ (fib-acc-2 42 0N 1N) 3N)
(fib-acc-2 142 0N 1N)

(defn noop
  [n]
  (if (= n 0)
    (let [v0 (* 1234567890N 1234567890N)
          v1 (* 1234567890N v0)
          v2 (* 1234567890N v1)
          v3 (* 1234567890N v2)
          v4 (* 1234567890N v3)]
      0)
    (do (noop (- n 1))
        (noop (- n 1)))))

(noop 25)

