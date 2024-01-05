(defn variadic1 
    ([a b & rest] rest)
    ([a] a))

(variadic1 1 2 3 4 5 6 7 8)
(variadic1 77)

(defn variadic2 
    ([a b & rest] (+ a b))
    ([a b] a))

(variadic2 1 2 3 4 5 6 7 8)
(variadic2 77 88)