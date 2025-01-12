(defn deep-recur [n sum]
  (if (= 0 n)
    sum
    (recur (- n 1) (+ sum n))))

(deep-recur 100000000 0)

