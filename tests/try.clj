(defn id [x] x)

(defn f []
  (let [a [11] b {"[22]" "ee"} c ["Kordian"] d [44]]
    (try (let [t {}] a (id a) t (/ 2 0) a)
         (catch RuntimeException e (let [run-e {}] c e run-e c))
         (catch Exception _ (let [just-e {}] d d just-e d))
         (finally (let [fin {}] d b fin b)))))

(defn g []
  (let [q {} w {} e {} r {} t {} y {} u {} i {} o {}]
    (try (try q
              (catch Exception _ w)
              (finally e))
         (catch Exception _ (try r
                                 (catch Exception _ t)
                                 (finally y)))
         (finally (try u
                       (catch Exception _ i)
                       (finally o))))))
