(let [a 42
      bag (reify
            clojure.lang.Counted
            (count [this] a)
            clojure.lang.Seqable
            (seq [this] (list a)))]
  [(count bag) (seq bag)])

(defprotocol Announcer (announce [this f]))

(let [counter (atom 5)
      announcer (reify
                  Announcer
                  (announce [this f]
                    (when-not (zero? @counter)
                      (swap! counter dec)
                      (f)
                      (recur f))))]
  (announce announcer #(println "Populus")))

(loop [x 10]
  (let [a (atom 10)
        v (reify
            clojure.lang.Counted
            (count [this] (if (zero? @a) 10 (do (swap! a dec) (recur)))))
        f (fn [t] (if (zero? t) t (recur (dec t))))]
    (if (zero? x)
      (f (count v))
      (recur (dec x)))))
