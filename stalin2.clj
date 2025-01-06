(defn average [x y]
  (/ (+ x y) 2))

(defn improve [guess x]
  (average guess (/ x guess)))

(defn good-enough? [guess x]
  (< (Math/abs (- (* guess guess) x)) 0.001))

(defn sqrt-iter [guess x]
  (let [good? (good-enough? guess x)
        improved (improve guess x)]
    (String/print "Guessing for")    
    (String/print x)
    (String/print "Guess: ")        
    (String/print guess)    
    (String/print "Good?: ")    
    (String/print good?)    
    (String/print "Improved: ")
    (String/print improved)    
    (if good?
      guess
      (recur improved
             x))))

(defn mysqrt [x]
  (sqrt-iter 1.0 x))

(defn int [x acc step]
  (String/print "INT:")    
  (String/print x)    
  (if (>= x 10000.0)
      acc
      (recur (+ x step)
           (+ acc (* step (mysqrt x)))
           step)))
           
(int 0.0 0.0 0.001)

;;(String/print 1000.777777223232)


;; x = 0.032
;; guess = 1
;; 
