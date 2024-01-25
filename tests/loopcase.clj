(let [a {} b {} c {}]
  (loop [t 1 u c v 0]
    (case t
      1 (recur (+ t t) u 111)
      2 (recur (+ t t) u 222)
      3 a
      4 (recur (+ t t) u 444)
      5 b
      6 (recur t u 666)
      7 ()
      t)))
