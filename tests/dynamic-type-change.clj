(defn f [] 8)

(defn g [] (if (= (f) 7) "excellent" "even better"))

(g)
(defn f [] 7)
(g)
(defn f [] "aa") 
(g)
