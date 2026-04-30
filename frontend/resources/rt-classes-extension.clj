{'clojure.lang.PersistentList
 {:static-fields
  {'creator (fn [& args] args)}}

 'clojure.lang.RT
 {:static-fns
  {'conj [{:type :clojure-function
           :symbol (fn [coll x]
                                        ; TODO: 
                                        ; static public IPersistentCollection conj(IPersistentCollection coll, Object x){
                                        ;        if(coll == null)
                                        ;                return new PersistentList(x);
                                        ;        return coll.cons(x);
                                        ; }
                     (let [lst (fn [& rest] rest)]
                       (if (= coll nil)
                         (lst)
                         (.cons coll x))))}]}}}

