(defn fib [x] (if (= x 1) 1 (if (= x 2) 1 (+ (fib (- x 1)) (fib (- x 2))))))

(fib 42)

(def x [1 2 3])

[x 2 3 5 "aa" :doda (fib 42)]
