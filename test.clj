(defn add [x y] (+ x y))

(add 3 5)
(add 3 4.0)

(defn nth [v] (+ 1 v))
(defn nth [v i] (v i))

(def some-vec [1 2 3 4 "aa" :kogo 7])

(nth some-vec 3)
