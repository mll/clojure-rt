(defn fib [x] (if (= x 1) 1 (if (= x 2) 1 (if (= x 43) nil (+ (fib (- x 1)) (fib (- x 2)))))))

;(def z fib)
;(def u [15])

;(z (u 0))
;(fib (u 0))
;(fib ([15] 0))

;(+ 3 (fib 5))
;(defn fib [x] "aa")
;(+ 3 (fib 5))

(fib 42)

(defn zombie [x]
    (if (>= x 10)
	(fib x)
	nil))
	
;(zombie 42)	