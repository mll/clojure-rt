
;(def x "aa")
;x
;(def x "bb")
;x
;(def x 3)
;x


;(defn zomo [x y] (- x y))
(defn zomo [x y] (+ x y))

(defn encapsulation []
    (zomo 10 7)
    (defn zomo [x y] "aa")
    (zomo 10 7))
    
(encapsulation)