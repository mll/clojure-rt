(def big-map
  (loop [m {} n 0]
    (if (= n 1000000)
      m
      (recur (assoc m n [n]) (+ n 1)))))

;; should be false
(loop [n 0]
  (if (= n 1000000)
    false
    (let [v (get big-map n)]
      (if (= v [n])
        (recur (+ n 1))
        n))))

;; should be nil
(get big-map 1234567)

(def big-map2
  (loop [m {} n 999999]
    (if (= n -1)
      m
      (recur (assoc m n [n]) (- n 1)))))

;; should be true
(= big-map big-map2)

(def big-map3
  (loop [m big-map n 0]
    (if (= n 1000000)
      m
      (recur (dissoc m n) (+ n 1)))))

;; should be true
(= big-map3 {})

(def big-map4
  (loop [m big-map n 1]
    (if (= n 1000000)
      m
      (recur (assoc m n [n]) (+ n 1)))))

;; should be false
(= big-map4 (dissoc big-map 0))

;; should be true
(= {} (dissoc (assoc {} 1 1) 1))

;; should be true (dissoc non-existent key)
(= {1 1} (dissoc {1 1} 2))

;; TODO: Find two non-equal values with equal hashes
(def h1 123)
(def h2 123)

;; should be true
(= (assoc (assoc {} h1 h1) h2 h2) (assoc (assoc {} h2 h2) h1 h1))

;; should be true
(= (dissoc (assoc (assoc {} h1 h1) h2 h2) h1) (assoc {} h2 h2))
