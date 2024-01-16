(let [a "very"
      b "much"
      c "not"]
  (letfn [#_neven?_0 (neven? #_1 [n] (if (zero? n) [a b] (nodd? #_0 (dec n))))
          #_nodd?_0 (nodd? #_1 [n] (if (zero? n) [c a] (neven? #_0 (dec n))))
          #_peven?_0 (peven? #_1 [n] (if (zero? n) "very much"
                                         (if (= 1 n) "not very"
                                             (peven? #_1 (- n 2)))))
          #_alfa_0 (alfa #_1 [] (beta #_1))
          #_beta_0 (beta #_1 [] 1)]
    [(nodd? #_0 11) (peven? #_0 12) (alfa #_0)]))
