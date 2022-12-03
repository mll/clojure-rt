(defn fib [x] (if (= x 1) [1]  (if (= x 2) [1] [(+ ((fib (- x 1)) 0) ((fib (- x 2)) 0))])))
(fib 42)


