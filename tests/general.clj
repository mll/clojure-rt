(defn zomo [a b] (+ a b))
(zomo 3 4)
;(println 3)
(+ 773.0 33.212)
(+ 2 17)
(+ (+ 2 3.0) (+ 5 8) (+ 3 4))
(+ 2 3 4)
(- 3 1)
(* 2 3)
(/ 12 6.0)

(Math/sin (+ 3 (* 2 3.0)))
(Math/exp 1.0)



(Math/sin (if true 2 1.0))
(if true 1 (Math/sin (+ 3 5)))
(if false 3.0 nil)

(if (= 3 5) 1.0 2)

'bb/aa
'7
(if true 6 7.0)
"aaa"
"kogutóóó"

:kanga

(= 1.0 7)
(= 2.0 3.0)
(if (= (if (= 7 (+ 3 4)) "zebra" :banga) nil) 1 1.0)
(if (= (if (= 7 (+ 3 4)) "zebra" :banga) "zebra") 1 :zonk)

(= 7 (+ 3 4))
(def x 7)


(* (+ (+ 1 2) x) (+ x (+ 1 2)))

(def x 5)

(* (+ (+ 1 2) x) (+ x (+ 1 2)))
:keyworsss 
(def x 5.0)

(defn maro ([x y & vars] (+ x 1))
    ([x] (+ x 2)))

(/ 4195835 3145727.0)

(maro 5)
(maro 10 10)

(defn fib [x] (if (= x 1) 1 (if (= x 2) 1 (+ (fib (- x 1)) (fib (- x 2))))))

(fib 40)

(defn factorial [y] (do (+ 3 5) (if (= y 1) y (* y (factorial (- y 1))))))

(factorial 20)






