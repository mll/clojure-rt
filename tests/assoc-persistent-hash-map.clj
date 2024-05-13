(loop [m {} n 100]
  (if (= 0 n)
    m
    (recur (assoc m [n] [n]) (- n 1))))