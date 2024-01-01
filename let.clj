(def lolo 3)

(defn with-let [x] (if (> x 1) (let [y 3 z x x 5] (+ x y z lolo) "lulo")))

(with-let 100)

