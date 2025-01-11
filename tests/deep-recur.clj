(defn deep-recur [n]
  (if (= 0 n)
    n
    (recur (- n 1))))

(deep-recur 100000000)
