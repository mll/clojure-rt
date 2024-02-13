(defn fib [x] 
    (if (= x 1) {:a 1}  
	(if (= x 2) {:a 1} 
	    {:a (+ (:a (fib (- x 1))) 
		   (:a (fib (- x 2))))})))
	    
(fib 42)


