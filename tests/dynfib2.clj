(defn fib [x] (if (= x 1) 1  (if (= x 2) 1 (if (= x 100) "aa" (+ (fib (- x 1)) (fib (- x 2)))))))
(fib 42)


