(defprotocol Container
  (cset [this o])
  (cget [this]))

(deftype LikeAtom [^:unsynchronized-mutable x]
  Container
  (cset [this o] (set! x o))
  (cget [this] x))

(def like-atom (LikeAtom. 3))

(cget like-atom)
(cset like-atom 4)
(cget like-atom)
