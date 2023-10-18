(defn with-let [x] (let [y 3 z x x 5] (+ x y z)))

(with-let 100)

