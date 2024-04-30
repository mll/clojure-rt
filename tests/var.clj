(def x "asd")
x
(def x 19)
(+ x 31)
(defonce x "ppp") ;; should be ignored
(+ x 31)


;; ;; Using overshadowed var 1

(def y 1)
(defn add-y [x] (+ x y))
(add-y 4) ;; should be 5
(def y 4)
(add-y 4) ;; should be 8

;; Using overshadowed var 2

(defn a [] 1)
(defn my-call [f] (fn [] (+ 1 (f))))
(def one (my-call a))
(def two (my-call #'a))
(defn a [] 2)

(one) ;; should be 2
(two) ;; should be 3

;; Type checking alone declares var

(defn f [x] (def qwerty x))
qwerty ;; #object[clojure.lang.Var$Unbound 0x382b2a30 "Unbound: #'user/qwerty"]

(defn g [x] (+ x "asd")) ;; is not rejected before executing
;; (g 3) ;; intended to fail, but without segfault!
