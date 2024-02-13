(defn make-vector
  [n]
  (let [gen (fn [vec n]
              (if (= n 0)
                vec
                (recur (clojure.lang.RT/conj vec n) (- n 1))))]
    (gen [] n)))

(let [a (make-vector 33)
      b (.asTransient a)
      c (.pop b)
      d (.persistent c)
      e (clojure.lang.RT/conj d 4)]
  [a b c d e])
