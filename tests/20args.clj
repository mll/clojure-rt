(defn bigarg [x1 x2 x3 x4 x5 x6 x7 x8 x9 x10 x11 x12 x13 x14 x15 x16 x17 x18 x19 x20] 
    (let [r1 (+ x1 x2)
	  r2 (+ r1 x3)
	  r3 (+ r2 x4)
	  r4 (+ r3 x5)
	  r5 (+ r4 x6)
	  r6 (+ r5 x7)
	  r7 (+ r6 x8)
	  r8 (+ r7 x9)
	  r9 (+ r8 x10)
	  r10 (+ r9 x11)
	  r11 (+ r10 x12)
	  r12 (+ r11 x13)
	  r13 (+ r12 x14)
	  r14 (+ r13 x15)
	  r15 (+ r14 x16)
	  r16 (+ r15 x17)
	  r17 (+ r16 x18)
	  r18 (+ r17 x19)
	  r19 (+ r18 x20)] 
      r19))

(= (bigarg 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20)
   210)

;; CHECK: Clojure JIT
