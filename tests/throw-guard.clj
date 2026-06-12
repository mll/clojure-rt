(let [thr (fn [] (throw (new java.lang.AssertionError "sample error")))
           f (fn [x y z] (+ (+ x y) z))]
      (f 10N 20N (f 30N 40N (thr))))