(def a "aa")

(+ (if (= a "a") 
      (+ a a) 
      7N) 
    a a)

[1N 2N 3N (+ 3N a)]

(+ a a)