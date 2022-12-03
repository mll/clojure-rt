(def fib 0)
(defn fib [x] x)


(fib 10)

(defn bifib [x] (fib x))

(bifib 10)

(defn fib [x] (if (= x 1) 1 (if (= x 2) 1 (+ (fib (- x 1)) (fib (- x 2))))))

(bifib 42)