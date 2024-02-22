;; (defn branch [n x y]
;;   (if (= n 0)
;;     (+ x y)
;;     (let [v0 (branch (- n 1) x y)
;;           v1 (branch (- n 1) v0 y)
;;           v2 (branch (- n 1) v1 y)
;;           v3 (branch (- n 1) v2 y)
;;           v4 (branch (- n 1) v3 y)
;;           v5 (branch (- n 1) v4 y)
;;           v6 (branch (- n 1) v5 y)
;;           v7 (branch (- n 1) v6 y)
;;           v8 (branch (- n 1) v7 y)
;;           v9 (branch (- n 1) v8 y)]
;;       v9)))

;; ;; Clojre execution time ~2,5s
;; ;; Our compiler compiles for 7 seconds can't generate module even if n = 0!
;; (branch 8 0N 2N)

;; LLVM error
(defn stick [n x y]
  (if (= n 0)
    (+ x y)
    (let [v0 (stick (- n 1) x y)
          v1 (stick (- n 1) v0 y)]
      v1)))

(stick 0 0N 2N)
