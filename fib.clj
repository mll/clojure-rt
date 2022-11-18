(defn fib [x] (if (= x 1) 1 (if (= x 2) 1 (+ (fib (- x 1)) (fib (- x 2))))))

(def z fib)

(z ([42] 0))

(+ 3 (fib 5))
(defn fib [x] "aa")
(+ 3 (fib 5))
