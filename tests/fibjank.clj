(in-ns 'clojure.core)
(def list clojure.lang.PersistentList/creator)
(in-ns 'user)

(defn fibonacci [n]
  (if (<= n 1)
    n
    (+ (fibonacci (- n 1))
       (fibonacci (- n 2)))))
       
       
(fibonacci 42)