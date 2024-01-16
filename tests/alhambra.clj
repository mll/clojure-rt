(let [a 10
      b 20
      f (fn alhambra
          ([x] (+ a x))
          ([x y] (alhambra x)))]
  (f 40)
  (f 50)
  b)
