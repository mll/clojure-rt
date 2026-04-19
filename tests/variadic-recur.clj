(defn f [x & y] (if (= x 0) y (recur (- x 1) [22N])))
(f 3N 2N 1N) 