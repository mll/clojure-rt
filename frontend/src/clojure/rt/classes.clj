(ns clojure.rt.classes)

;; TODO: Construct class hierarchy in this file
;; TODO: java.lang.Object would be actually clojure.rt.classes.java.lang.Object?

(deftype* ^{:superclass nil ;; java.lang.Object
            :methods [;; (clone [this])
                      ;; (equals [this other])
                      ;; (finalize [this])
                      ;; (getClass [this])
                      ;; (hashCode [this])
                      ;; (notify [this])
                      ;; (notifyAll [this])
                      (toString [this] (clojure.lang.RT/Deftype_toString this))
                      ;; (wait [this]) ;; TODO: Final?
                      (wait [this])
                      (wait [this timeout])
                      (wait [this timeout & nanos])]}
  java.lang.Object ;; name
  java.lang.Object ;; class.name
  [] ;; fields
  )

