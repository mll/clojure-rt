(defn over9000
  [x]
  (if (< 9000 x)
    x
    (over9000 (+ x x))))

(over9000 1)

(defn over9000-recur
  [x]
  (if (< 9000 x)
    x
    (recur (+ x x))))

(over9000-recur 1)

;; CHECK: define i64 @fn_2_J_J
;; CHECK-NOT: load atomic ptr
;; CHECK: }
