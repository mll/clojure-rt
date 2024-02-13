
(defn make-vector
  [n]
  (let [gen (fn gen [vec n]
              (if (= n 0)
                vec
                (recur (clojure.lang.RT/conj vec n) (- n 1))))]
    (gen [] n)))

;;;;;;;; N = 1 OK

(let [a (make-vector 1)
      b (clojure.lang.RT/pop a)]
  a)
(let [a (make-vector 1)
      b (clojure.lang.RT/pop a)]
  b)
(let [a (make-vector 1)
      b (clojure.lang.RT/pop a)]
  [a b])

;;;;;;;; N = 2 OK

(let [a (make-vector 2)
      b (clojure.lang.RT/pop a)]
  a)
(let [a (make-vector 2)
      b (clojure.lang.RT/pop a)]
  b)
(let [a (make-vector 2)
      b (clojure.lang.RT/pop a)]
  [a b])

;;;;;;;; N = 33 OK

(let [a (make-vector 33)
      b (clojure.lang.RT/pop a)]
  a)
(let [a (make-vector 33)
      b (clojure.lang.RT/pop a)]
  b)
(let [a (make-vector 33)
      b (clojure.lang.RT/pop a)]
  [a b])

;;;;;;;; N = 34 OK

(let [a (make-vector 34)
      b (clojure.lang.RT/pop a)]
  a)
(let [a (make-vector 34)
      b (clojure.lang.RT/pop a)]
  b)
(let [a (make-vector 34)
      b (clojure.lang.RT/pop a)]
  [a b])

;;;;;;;; N = 2 * 32 + 1 = 65, root reusable OK

(let [a (make-vector 65)
      b (clojure.lang.RT/pop a)]
  a)
(let [a (make-vector 65)
      b (clojure.lang.RT/pop a)]
  b)
(let [a (make-vector 65)
      b (clojure.lang.RT/pop a)]
  [a b])

;;;;;;;; N = 2 * 32 + 1 = 65, root not reusable OK

(let [x (make-vector 66)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [a x])
(let [x (make-vector 66)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [b x])
(let [x (make-vector 66)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [[a b] x])

;;;;;;;; N = 3 * 32 + 1 = 97, root reusable OK

(let [a (make-vector 97)
      b (clojure.lang.RT/pop a)]
  a)
(let [a (make-vector 97)
      b (clojure.lang.RT/pop a)]
  b)
(let [a (make-vector 97)
      b (clojure.lang.RT/pop a)]
  [a b])

;;;;;;;; N = 3 * 32 + 1 = 97, root not reusable OK

(let [x (make-vector 98)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [a x])
(let [x (make-vector 98)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [b x])
(let [x (make-vector 98)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [[a b] x])

;;;;;;;; N = 32 * 32 + 32 + 1 = 1057, root reusable OK

(let [a (make-vector 1057)
      b (clojure.lang.RT/pop a)]
  a)
(let [a (make-vector 1057)
      b (clojure.lang.RT/pop a)]
  b)
(let [a (make-vector 1057)
      b (clojure.lang.RT/pop a)]
  [a b])

;;;;;;;; N = 32 * 32 + 32 + 1 = 1057, root not reusable

(let [x (make-vector 1058)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [a x])
(let [x (make-vector 1058)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [b x])
(let [x (make-vector 1058)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [[a b] x])

;;;;;;;; N = 32 * 32 + 2 * 32 + 1 = 1089, root reusable OK

(let [a (make-vector 1089)
      b (clojure.lang.RT/pop a)]
  a)
(let [a (make-vector 1089)
      b (clojure.lang.RT/pop a)]
  b)
(let [a (make-vector 1089)
      b (clojure.lang.RT/pop a)]
  [a b])

;;;;;;;; N = 32 * 32 + 2 * 32 + 1 = 1089, root not reusable

(let [x (make-vector 1090)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [a x])
(let [x (make-vector 1090)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [b x])
(let [x (make-vector 1090)
      a (clojure.lang.RT/pop x)
      b (clojure.lang.RT/pop a)]
  [[a b] x])
