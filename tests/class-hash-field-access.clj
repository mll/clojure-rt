(deftype* user/UU UU [qwe wer ert rty tyu yui uio iop])

;; 3,7s without hash guesses
;; 4s with hash guesses
(loop [n 2000000 ret []]
  (if (= n 0)
    ret
    (recur (- n 1) (UU. 1 2 3 4 5 6 7 8))))

(def x (UU. 1 2 3 4 5 6 7 8))

(defn f [x] [(.qwe x) (.wer x) (.ert x) (.rty x) (.tyu x) (.yui x) (.uio x) (.iop x)])

;; 6,6s without hash guess
;; 5s with hash guess
(loop [n 2000000 ret []]
  (if (= n 0)
    ret
    (recur (- n 1) (f x))))
