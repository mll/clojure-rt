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
  AnotherObject ;; name
  AnotherObject ;; class.name
  [] ;; fields
  ;; :implements [] ;; TODO
  )

;; (def obj (AnotherObject.)) ;; TODO
;; (.toString obj) ;; TODO
