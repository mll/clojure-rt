(defn variadic 
    ([a b & rest] (+ a b))
    ([a] a))

(variadic 1 2 3 4 5 6 7 8)