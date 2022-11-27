(def x {:a 1 :b 2 :c 3})
x

(x :g)
(x :b)

(:g x)
(:b x)

(:b {:a 1 :b 2 :c 3})
(:g {:a 1 :b 2 :c 3})
({:a 1 :b 2 :c 3} :g)
({:a 1 :b 2 :c 3} :b)