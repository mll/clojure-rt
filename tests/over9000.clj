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

(defn over9000-recur-1
  ([x]
   (if (< 9000 x)
     x
     (recur (+ x x))))
  ([x y] 123))

(over9000-recur-1 1)
(over9000-recur-1 1 2)

(defn over9000-recur-2
  ([x y] 123)
  ([x]
   (if (< 9000 x)
     x
     (recur (+ x x)))))

(over9000-recur-2 1)
(over9000-recur-2 1 2)
