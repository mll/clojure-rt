(defprotocol Pendulum
  (swing [this init] "Given initial pendulum position, get its final position"))

(defrecord ConstSwing [plus minus]
  Pendulum
  (swing [_this init]
    (println "max local tilt at" init)
    (if (<= plus init minus)
      init
      (if (< init plus)
        (recur (+ init plus)) ;; in method, this is automatically passed in recur!
        (recur (- init minus))))))

(extend-protocol Pendulum
  clojure.lang.PersistentHashSet
  (swing [this init]
    (or (this init) 0)))

(let [pendulum (ConstSwing. 77 80)]
  (swing pendulum 100)
  (swing pendulum 101)
  (swing pendulum 102))
(swing #{123 124 125} 111)
(swing #{123 124 125} 125)
