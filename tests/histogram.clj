(def *histogram-heights* '(3 5 7 10 21 38 22 54 1 3 2 5))

(defn car [iterable] (first iterable))
(defn cdr [iterable] (rest iterable))
(defn cadr [iterable] (second iterable))

(defn execute-step [remaining-list stack maximal-area position] 
  (let [height (or (cadr (car stack)) 0)
    	start  (car (car stack))
        current-height (or (car remaining-list) 0)]
    (if (and stack (or (not remaining-list) (< current-height height))) 
      (execute-step remaining-list (cdr stack) (max maximal-area (* height  (- position start))) position) ; remember maximal, pop and call again
      (if (not remaining-list) maximal-area ; we are at the end, return best result
          (if (or (not stack) (> current-height height)) 
            (execute-step (cdr remaining-list) (cons (list position current-height) stack) maximal-area (+ position 1) ) ; we have encountered possible better result, push to the stack and go further on
            (execute-step (cdr remaining-list) stack maximal-area (+ 1 position))))))) ; nothing interesting happened, just go ahead



(execute-step *histogram-heights* (list) 0.0 1)

