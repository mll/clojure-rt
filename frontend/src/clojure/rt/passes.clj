(ns clojure.rt.passes
  (:refer-clojure :exclude [compile])
  (:require [clojure.set :as set]
            [clojure.tools.analyzer.ast :as ast]
            [clojure.tools.analyzer.passes
             [uniquify :refer [uniquify-locals]]
             [collect-closed-overs :refer [collect-closed-overs]]
             [trim :refer [trim]]]
            [clojure.walk :refer [postwalk]]))

(defn remove-env
  {:pass-info {:walk :pre :depends #{}}}
  [ast]
  (if (and (map? ast)
           (contains? ast :env))
    (-> ast
        (update-in [:env] #(dissoc % :context :locals)))
    ast))

(defn simplify-closed-overs
  {:pass-info {:walk :pre :depends #{#'collect-closed-overs}}}
  [ast]  
  (if (and (map? ast)
           (contains? ast :closed-overs))    
    (-> ast 
        (update :closed-overs vals)
        (update :closed-overs #(map remove-env %)))
    ast))


(defn rewrite-loops
  "Rewrite loop nodes `(loop [bindings] body)` to `(let [G (loop [bindings] body)] G)` and tag those lets."
  {:pass-info {:walk :post :before #{#'uniquify-locals}}}
  [ast]
  (if (= :loop (:op ast))
    (let [new-var (gensym)]
      {:loop-let true
       :op :let
       :bindings [{:op :binding
                   :children [:init]
                   :form new-var
                   :name new-var
                   :init ast
                   :local :let}]
       :body {:children []
              :op :local
              :form new-var
              :name new-var
              :local :let}
       :form `(~'let* [~new-var ~(:form ast)] ~new-var) ;; outer forms are not rewritten
       :children [:bindings :body]})
    ast))

(defmulti -fresh-vars
  (fn [{:keys [op]} _]
    (assert (not (#{:host-interop} op))
            (str "-fresh-vars: " op " not yet implemented"))
    (assert (not (#{:case-test} op))
            (str "-fresh-vars: " op " should never occur"))
    op))
    


(defmethod -fresh-vars :local
  [node _recur-this]
  (assoc node :fresh #{(:name node)}))

(defmethod -fresh-vars :fn-method
  [node recur-this]
  (let [updated-node (ast/update-children node #(-fresh-vars % recur-this))]
    (-> updated-node
        (assoc :fresh (set/difference (:fresh (:body updated-node))
                                      (set (map :name (:params updated-node))))))))

(defmethod -fresh-vars :fn
  [node recur-this]
  (let [updated-node (ast/update-children node #(-fresh-vars % recur-this))
        local-binding (-> node :local :name)
        all-fresh (cond-> (->> updated-node ast/children (map :fresh) (apply set/union))
                    local-binding (disj local-binding))]
    (-> updated-node
        (assoc :fresh all-fresh))))

(defn let-fresh-vars
  [node recur-this]
  (let [updated-body (-fresh-vars (:body node) recur-this)
        [reversed-updated-bindings fresh]
        (->> node
             :bindings
             reverse
             (reduce (fn [[new-bindings fresh] binding]
                       (let [updated-init (-fresh-vars (:init binding) recur-this)
                             binding-fresh (set/union (disj fresh (:name binding))
                                                      (:fresh updated-init))
                             updated-binding (assoc binding
                                                    :init updated-init
                                                    :fresh binding-fresh)]
                         [(conj new-bindings updated-binding) binding-fresh]))
                     [[] (:fresh updated-body)]))
        updated-bindings (vec (reverse reversed-updated-bindings))]
    (-> node
        (assoc :body updated-body
               :bindings updated-bindings
               :fresh fresh))))

(defmethod -fresh-vars :let [node recur-this] (let-fresh-vars node recur-this))

(defmethod -fresh-vars :letfn
  [node recur-this]
  (let [updated-node (let-fresh-vars node recur-this)
        fn-names (set (map :name (:bindings node)))]
    (update updated-node :fresh set/difference fn-names)))

(defmethod -fresh-vars :loop
  [node recur-this]
  (let [updated-body (-fresh-vars (:body node) recur-this)
        [reversed-updated-bindings fresh]
        (->> node
             :bindings
             reverse
             (reduce (fn [[new-bindings fresh] binding]
                       (let [updated-init (-fresh-vars (:init binding) recur-this)
                             binding-fresh (set/union (disj fresh (:name binding))
                                                      (:fresh updated-init))
                             updated-binding (assoc binding
                                                    :init updated-init
                                                    :fresh binding-fresh)]
                         [(conj new-bindings updated-binding) binding-fresh]))
                     [[] (:fresh updated-body)]))
        updated-bindings (vec (reverse reversed-updated-bindings))]
    (-> node
        (assoc :body updated-body
               :bindings updated-bindings
               :fresh fresh))))

(defmethod -fresh-vars :do
  [node recur-this]
  (let [updated-ret (-fresh-vars (:ret node) recur-this)
        [reversed-updated-statements fresh]
        (->> node
             :statements
             reverse
             (reduce (fn [[new-statements fresh] statement]
                       (let [updated-statement (-fresh-vars statement recur-this)
                             binding-fresh (set/union fresh (:fresh updated-statement))
                             updated-statement (assoc updated-statement :fresh binding-fresh)]
                         [(conj new-statements updated-statement) binding-fresh]))
                     [[] (:fresh updated-ret)]))
        updated-statements (vec (reverse reversed-updated-statements))]
    (-> node
        (assoc :ret updated-ret
               :statements updated-statements
               :fresh fresh))))

(defmethod -fresh-vars :case
  [node recur-this]
  (let [updated-thens (vec (map #(-fresh-vars % recur-this) (:thens node)))
        updated-default (some-> node :default (-fresh-vars recur-this))
        all-fresh (apply set/union (:fresh updated-default) #{(-> node :test :name)} (map (comp :fresh :then) updated-thens))]
    (cond-> node
      updated-default (assoc :default updated-default)
      true (assoc :thens updated-thens
                  :fresh all-fresh))))

(defmethod -fresh-vars :catch
  [node recur-this]
  (let [updated-body (-fresh-vars (:body node) recur-this)
        fresh (disj (:fresh updated-body) (:name (:local node)))]
    (-> node
        (assoc :body updated-body
               :fresh fresh))))

(defmethod -fresh-vars :deftype
  [node recur-this]
  (let [field-names (->> node :fields (map :name) set)
        update-method (fn [method-node]
                        (let [updated-method-node (-> method-node
                                                      (-fresh-vars recur-this)
                                                      (update :fresh set/difference field-names))]
                          (assert (empty? (:fresh updated-method-node))
                                  (str "Record method in deftype should not have any free variables: "
                                       (:fresh updated-method-node)
                                       (:form updated-method-node)))
                          updated-method-node))]
    (-> node
        (update :methods #(mapv update-method %))
        (assoc :fresh #{}))))

(defmethod -fresh-vars :reify
  [node recur-this]
  (let [updated-methods (mapv #(-fresh-vars % recur-this) (:methods node))]
    (-> node
        (assoc :methods updated-methods
               :fresh (apply set/union (map :fresh updated-methods))))))

(defmethod -fresh-vars :method
  [node recur-this]
  (let [this (:this node)
        updated-body (->> :local
                          (assoc this :op)
                          (assoc recur-this (:loop-id node))
                          (-fresh-vars (:body node)))
        args (conj (->> node :params (map :name) set) (:name this))]
    (-> node
        (assoc :body updated-body
               :fresh (set/difference (:fresh updated-body) args)))))

(defmethod -fresh-vars :recur
  [node recur-this]
  ;; Special case: in defrecord/reify methods, you are not supposed to pass this as an argument
  ;; (it is passed implicitly), only remaining arguments (see pendulum.clj as an example)
  (let [updated-node (ast/update-children node #(-fresh-vars % recur-this))
        all-fresh (->> updated-node ast/children (map :fresh) (apply set/union))
        this (recur-this (node :loop-id))
        all-fresh (cond-> all-fresh this (conj (:name this)))]
    (cond-> updated-node
      this (assoc :recur-this (-fresh-vars this recur-this))
      true (assoc :fresh all-fresh))))

;; Handled by default:
;; binding, case-then, const, def, fn, import, instance-call, instance-field, instance?, invoke, keyword-invoke, map,
;; monitor-enter, monitor-exit, new, prim-invoke, protocol-invoke, quote, recur, set, set!,
;; static-call, static-field, the-var, throw, try, var, vector, with-meta
(defmethod -fresh-vars :default
  [node recur-this]
  (let [updated-node (ast/update-children node #(-fresh-vars % recur-this))
        all-fresh (->> updated-node ast/children (map :fresh) (apply set/union))]
    (-> updated-node
        (assoc :fresh all-fresh))))

(defn fresh-vars
  {:pass-info {:walk :none :depends #{#'uniquify-locals #'trim}}}
  [ast]
  (-fresh-vars ast {}))


(defmulti -memory-management-pass
  (fn [{:keys [fresh op form]} borrowed owned unwind-owned]
    (assert (not (#{:host-interop} op))
            (str "-memory-management-pass: " op " not yet implemented"))
    (assert (not (#{:case-test :fn-method :method} op))
            (str "-memory-management-pass: " op " should never occur"))
    ;; (println "-memory-management-pass" op "fresh" fresh "borrowed" borrowed "owned" owned "unwind-owned" unwind-owned)
    (assert (empty? (set/intersection borrowed owned))
            (str "-memory-management-pass: Invariant violation in " op ": borrowed " borrowed
                 " and owned " owned " have nonempty intersection"))
    (assert (set/subset? owned fresh)
            (str "-memory-management-pass: Invariant violation in " op ": owned " owned
                 " not a subset of fresh " fresh))
    (assert (set/subset? unwind-owned (set/union borrowed owned))
            (str "-memory-management-pass: Invariant violation in " op ": unwind-owned " unwind-owned
                 " not a subset of borrowed " borrowed " + owned " owned))
    (assert (set/subset? fresh (set/union borrowed owned))
            (str "-memory-management-pass: Invariant violation in " op ": fresh " fresh
                 " not a subset of borrowed " borrowed " + owned " owned))
    op))

(defn update-drop-memory
  [node key name->amount]
  (update node key #(merge-with + % name->amount)))

(defn drop-vars
  [node names]
  (update-drop-memory node :drop-memory (->> names (map #(vector % -1)) (into {}))))

(defn dup-vars
  [node names]
  (update-drop-memory node :drop-memory (->> names (map #(vector % 1)) (into {}))))

(defn set-unwind
  [node names]
  (update-drop-memory node :unwind-memory (->> names (map #(vector % -1)) (into {}))))

;; In paper: SVAR, SVAR-DUP

(defn variable-usage
  [node owned name]
  (or (and (= owned #{name}) node)
      (and (empty? owned)
           ;; (borrowed name) ;; disabled due to how defs work
           (dup-vars node [name]))
      (assert false (str "Invariant violation: unexpected SVAR case " owned))))

(defmethod -memory-management-pass :var
  [node _borrowed _owned unwind-owned]
  (set-unwind node unwind-owned)
  #_(variable-usage node owned (:form node)))

(defmethod -memory-management-pass :local
  [node _borrowed owned unwind-owned]
  (-> node
      (variable-usage owned (:name node))
      (set-unwind unwind-owned)))

;; In paper: SAPP, adapted to modify function and all arguments at once (see also SCON)
;; This also applies for any sequence of unnamed values

(defn application-usage
  [nodes borrowed owned unwind-owned]
  (if (empty? nodes)
    nodes
    (let [fresh-n (map :fresh nodes) ;; vector: i -> fv(v_i)
          owned-last (set/intersection owned (last fresh-n)) ;; Gamma_n
          ;; vector: i == Gamma_i ;; in reduce loop, vector is generated from the end
          owned-n (->> fresh-n
                       reverse
                       rest
                       (reduce (fn [[owned-n owned-diff] fresh-i] ;; i = n - 1 .. 1
                                 (let [owned-diff (set/difference owned-diff (first owned-n)) ;; owned-diff = Gamma - Gamma_(i + 1) - ... - Gamma_n
                                       owned-i (set/intersection owned-diff fresh-i)] ;; Gamma_i
                                   [(conj owned-n owned-i) owned-diff]))
                               [(list owned-last) owned])
                       first
                       vec)
          ;; vector: i -> Delta + Gamma_(i + 1) + ... + Gamma_n
          borrowed-n (->> owned-n
                          rest
                          reverse
                          (reduce (fn [borrowed owned-i]
                                    (conj borrowed (set/union (first borrowed) owned-i)))
                                  (list borrowed))
                          vec)
          [evaluated-nodes _unwind-owned]
          (reduce (fn [[updated-nodes unwind-owned] [borrowed owned node]]
                    (let [updated-node (-memory-management-pass node borrowed owned unwind-owned)
                          rest-unwind-owned (set/difference unwind-owned owned)]
                      [(conj updated-nodes updated-node) rest-unwind-owned]))
                  [[] unwind-owned]
                  (map vector borrowed-n owned-n nodes))]
      evaluated-nodes)))

(defmethod -memory-management-pass :invoke
  [node borrowed owned unwind-owned]
  (let [nodes (concat [(:fn node)] (:args node))
        [updated-fn & updated-args] (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :fn updated-fn
               :args (vec updated-args)))))

(defmethod -memory-management-pass :prim-invoke
  [node borrowed owned unwind-owned]
  (let [nodes (concat [(:fn node)] (:args node))
        [updated-fn & updated-args] (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :fn updated-fn
               :args (vec updated-args)))))

(defmethod -memory-management-pass :protocol-invoke
  [node borrowed owned unwind-owned]
  (let [nodes (concat [(:protocol-fn node) (:target node)] (:args node))
        [updated-protocol-fn updated-target & updated-args] (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :protocol-fn updated-protocol-fn
               :target updated-target
               :args (vec updated-args)))))

(defmethod -memory-management-pass :instance-call
  [node borrowed owned unwind-owned]
  (let [nodes (concat [(:instance node)] (:args node))
        [updated-instance & updated-args] (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :instance updated-instance
               :args (vec updated-args)))))

(defmethod -memory-management-pass :keyword-invoke
  [node borrowed owned unwind-owned]
  (let [nodes (map node [:keyword :target])
        [updated-keyword updated-target] (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :keyword updated-keyword
               :target updated-target))))

(defmethod -memory-management-pass :static-call
  [node borrowed owned unwind-owned]
  (let [nodes (:args node)
        updated-nodes (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :args updated-nodes))))

(defmethod -memory-management-pass :new
  [node borrowed owned unwind-owned]
  (let [nodes (:args node)
        updated-nodes (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :args updated-nodes))))

(defmethod -memory-management-pass :do
  ;; Earlier pass guarantees that there is (initially) at least one statement
  [node borrowed owned unwind-owned]
  (let [nodes (concat (:statements node) [(:ret node)])
        updated-nodes (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :statements (vec (butlast updated-nodes))
               :ret (last updated-nodes)))))

(defmethod -memory-management-pass :recur
  [node borrowed owned unwind-owned]
  (let [nodes (:exprs node)
        this (:recur-this node)
        nodes (cond->> nodes this (concat [this]))
        updated-nodes (application-usage nodes borrowed owned unwind-owned)
        [updated-this updated-exprs] (if this
                                       [(first updated-nodes) (vec (rest updated-nodes))]
                                       [nil updated-nodes])]
    (cond-> node
        true (set-unwind unwind-owned)
        updated-this (assoc :recur-this updated-this)
        true (assoc :exprs updated-exprs))))

(defmethod -memory-management-pass :set!
  [node borrowed owned unwind-owned]
  (let [nodes (map node [:target :val])
        [updated-target updated-val] (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :target updated-target
               :val updated-val))))

(defmethod -memory-management-pass :with-meta
  [node borrowed owned unwind-owned]
  (let [nodes (map node [:meta :expr])
        [updated-meta updated-expr] (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :meta updated-meta
               :expr updated-expr))))

(defmethod -memory-management-pass :map
  ;; Order of evaluation is key1, val1, key2, val2, ...
  [node borrowed owned unwind-owned]
  (let [nodes (->> [:keys :vals] (map node) (apply interleave))
        updated-nodes (application-usage nodes borrowed owned unwind-owned)
        [updated-keys updated-vals] (if (seq updated-nodes)
                                      (apply mapv vector (partition 2 updated-nodes))
                                      [[] []])]
    (-> node
        (set-unwind unwind-owned)
        (assoc :keys updated-keys
               :vals updated-vals))))

(defmethod -memory-management-pass :set
  [node borrowed owned unwind-owned]
  (let [updated-nodes (application-usage (:items node) borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :items updated-nodes))))

(defmethod -memory-management-pass :vector
  [node borrowed owned unwind-owned]
  (let [updated-nodes (application-usage (:items node) borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :items updated-nodes))))

;; In paper: SLAM and SLAM-DROP, merged to modify function with multiple arities

;; fn/fn-method/method is NOT expected to have drop/unwind-memory, all information is passed to the body
(defn fn-method-memory-management-pass
  [node record-fields]
  (let [fn-fresh (:fresh node) ;; ys
        this (:name (:this node)) ;; defrecord/reify method
        args (cond-> (set (map :name (:params node))) this (conj this))
        used (->> node :body :fresh (set/intersection args)) ;; x in fv(e)
        record-fields-used (->> node :body :fresh (set/intersection record-fields))
        unused (set/difference args used) ;; x not in fv(e)
        record-fields-unused (set/difference record-fields record-fields-used)
        updated-body (-memory-management-pass (:body node) #{} (set/union fn-fresh used record-fields-used) used)]
    (-> node
        (assoc :body updated-body)
        (update :body drop-vars unused)
        (update :body drop-vars record-fields-unused)
        (update :body dup-vars fn-fresh))))

;; :drop/unwind-memory defines memory management at the moment of DEFINING a function
;; When the function is dropped (or an exception is thrown), it should also drop (unwind) variables in :closed-overs
(defmethod -memory-management-pass :fn
  [node _borrowed owned unwind-owned]
  (let [local-binding (-> node :local :name)
        updated-methods (mapv #(fn-method-memory-management-pass % #{}) (:methods node))
        fn-used-locals (cond-> (apply set/union (map :fresh updated-methods))
                         true (set/difference owned) ;; Delta_1
                         local-binding (disj local-binding))]
    (-> node
        (assoc :methods updated-methods)
        (dup-vars fn-used-locals)
        (set-unwind unwind-owned)
        (set-unwind fn-used-locals))))

(defmethod -memory-management-pass :method
  [node _borrowed _owned _unwind-owned]
  (fn-method-memory-management-pass node #{}))

(defmethod -memory-management-pass :reify
  [node _borrowed owned unwind-owned]
  (let [updated-methods (mapv #(fn-method-memory-management-pass % #{}) (:methods node))
        fn-used-locals (set/difference (apply set/union (map :fresh updated-methods)) owned)]
    (-> node
        (assoc :methods updated-methods)
        (dup-vars fn-used-locals)
        (set-unwind unwind-owned)
        (set-unwind fn-used-locals))))

(defmethod -memory-management-pass :deftype
  [node _borrowed _owned _unwind-owned]
  (let [fields (set (map :name (:fields node)))
        updated-methods (mapv #(fn-method-memory-management-pass % fields) (:methods node))]
    (-> node
        (assoc :methods updated-methods))))

;; In paper: SBIND and SBIND-DROP, adapted to handle multiple bindings in let

(defn node-with-bindings-memory-management-pass
  [node borrowed owned unwind-owned]
  (if (empty? (:bindings node))
    (do (when (= :loop (:op node))
          (assert (set/subset? owned (:bound-by-this node))
                  (str "Loop body must not own any local variable, except for loop bindings! " owned)))
        (if (= :letfn (:op node))
          (let [fn-names (:bound-by-this node)
                fn-used (set/intersection fn-names (:fresh (:body node)))
                fn-unused (set/difference fn-names fn-used)
                borrowed (set/difference borrowed fn-names)
                owned (set/union owned fn-used)
                unwind-owned (set/difference unwind-owned fn-unused)]
            (update node :body #(-> %
                                    (-memory-management-pass borrowed owned unwind-owned)
                                    (drop-vars fn-unused))))
          (update node :body -memory-management-pass borrowed owned unwind-owned)))
    (let [[binding & bindings] (:bindings node)
          local-name (:name binding) ;; x
          remainder-fresh (if (seq bindings) (:fresh (first bindings)) (:fresh (:body node))) ;; fv(e_2)
          local-name-used (remainder-fresh local-name) ;; x in fv(e_2)
          remainder-owned (set/intersection owned (disj remainder-fresh local-name)) ;; Gamma_2
          remainder-unwind-owned (set/intersection unwind-owned (set/union borrowed remainder-owned))
          updated-init (-memory-management-pass (:init binding)
                                                (set/union borrowed remainder-owned)
                                                (set/difference owned remainder-owned)
                                                unwind-owned)
          updated-binding (assoc binding :init updated-init)
          updated-node (-> node
                           (assoc :bindings bindings :fresh remainder-fresh)
                           (update :bound-by-this set/union #{local-name}))
          updated-remainder (node-with-bindings-memory-management-pass
                             updated-node
                             borrowed
                             (cond-> remainder-owned (and local-name-used (not= :letfn (:op node))) (conj local-name))
                             (cond-> remainder-unwind-owned local-name-used (conj local-name)))
          drop-local-updated-remainder (if (or local-name-used (= :letfn (:op node)))
                                         updated-remainder
                                         (let [ks (if (seq bindings) [:bindings 0] [:body])]
                                           (update-in updated-remainder ks drop-vars [local-name])))]
      (-> drop-local-updated-remainder
          (assoc :fresh (:fresh node))
          ;; (set-unwind unwind-owned) ;; refer to individual bindings/body
          (update :bindings #(vec (concat [updated-binding] %)))))))

(defmethod -memory-management-pass :loop
  [node borrowed owned unwind-owned]
  (node-with-bindings-memory-management-pass node borrowed owned unwind-owned))

(defmethod -memory-management-pass :let
  [node borrowed owned unwind-owned]
  (if (:loop-let node)
    ;; Let is of form `(let [G (loop [bindings] body)] G)`.
    ;; Decorate loop body under assumption that all local variables are borrowed
    ;; (they are owned by the next loop iteration) and drop them before let body G.
    (let [owned-by-loop-body (set/intersection owned (-> node :bindings first :init :body :fresh))]
      (-> node
          (update :body drop-vars owned-by-loop-body)
          (node-with-bindings-memory-management-pass
           (set/union borrowed owned-by-loop-body)
           (set/difference owned owned-by-loop-body)
           unwind-owned)))
    ;; In standard case, earlier pass guarantees that there is (initially) at least one binding
    (node-with-bindings-memory-management-pass node borrowed owned unwind-owned)))

(defmethod -memory-management-pass :letfn
  [node borrowed owned unwind-owned]
  (let [fn-names (set (map :name (:bindings node)))
        borrowed (set/union borrowed fn-names)
        owned (set/difference owned fn-names)]
    (node-with-bindings-memory-management-pass node borrowed owned unwind-owned)))

(defmethod -memory-management-pass :try
  ;; Earlier pass (trim) guarantees that there is at least one catch OR finally block
  [node borrowed owned unwind-owned]
  (let [finally-owned (set/intersection owned (:fresh (:finally node)))
        each-catch-owned (map #(-> % :fresh (set/intersection owned) (set/difference finally-owned)) (:catches node))
        all-catches-owned (apply set/union each-catch-owned)
        each-catch-disowned (mapv #(set/difference all-catches-owned %) each-catch-owned)
        try-owned (-> owned
                      (set/intersection (:fresh (:body node)))
                      (set/difference (set/union finally-owned all-catches-owned)))
        all-catches-unwind-owned (set/difference unwind-owned try-owned)
        try-borrowed (set/union borrowed (set/difference owned try-owned))
        updated-body (-memory-management-pass (:body node) try-borrowed try-owned #{})
        updated-catches (mapv (fn [catch-node catch-owned catch-disowned]
                                (-> catch-node
                                    (-memory-management-pass (set/difference try-borrowed catch-owned)
                                                             catch-owned
                                                             (set/difference all-catches-unwind-owned catch-disowned))
                                    (drop-vars catch-disowned)))
                              (:catches node)
                              each-catch-owned
                              each-catch-disowned)
        finally-unwind-owned (set/difference all-catches-unwind-owned all-catches-owned)
        updated-finally (some-> node
                                :finally
                                (-memory-management-pass borrowed finally-owned finally-unwind-owned)
                                #_(drop-vars all-catches-owned))]
    (cond-> node
      true (assoc :body updated-body)
      true (assoc :catches updated-catches)
      updated-finally (assoc :finally updated-finally)
      true (assoc :all-catches-owned all-catches-owned) ;; drop these variables if execution went from try block directly to finally
      true (set-unwind unwind-owned))))

(defmethod -memory-management-pass :catch
  [node borrowed owned unwind-owned]
  (let [exception-name (:name (:local node))
        body-fresh (:fresh (:body node))
        exception-used (body-fresh exception-name)
        body-owned (cond-> owned
                     true (set/intersection body-fresh)
                     exception-used (conj exception-used))
        body-unwind-owned (cond-> unwind-owned exception-used (conj exception-used))
        updated-body (-memory-management-pass (:body node) borrowed body-owned body-unwind-owned)
        drop-exception-updated-body (cond-> updated-body (not exception-used) (drop-vars [exception-name]))]
    (-> node
        (assoc :body drop-exception-updated-body))))

;; In paper: SMATCH, adapted

(defmethod -memory-management-pass :if
  [node borrowed owned unwind-owned]
  (let [branches [:then :else]
        ;; bv(p_i) is always empty
        owned-in-branch (->> branches
                             (map (fn [branch] [branch (set/intersection owned (:fresh (branch node)))]))
                             (into {})) ;; Gamma_i
        owned-in-branches (apply set/union (vals owned-in-branch))
        owned-in-condition (set/difference owned owned-in-branches)
        updated-test (-memory-management-pass (:test node)
                                              (set/union borrowed owned-in-branches)
                                              owned-in-condition
                                              unwind-owned)
        unwind-owned-in-branches (set/difference unwind-owned owned-in-condition)
        disowned-in-branch (->> branches
                                (map (fn [branch] [branch (set/difference owned-in-branches (branch owned-in-branch))]))
                                (into {})) ;; Gamma'_i
        updated-branches (->> branches
                              (map (fn [branch] [branch (-> node
                                                            branch
                                                            (-memory-management-pass
                                                             borrowed
                                                             (owned-in-branch branch)
                                                             (set/difference unwind-owned-in-branches (disowned-in-branch branch)))
                                                            (drop-vars (disowned-in-branch branch)))]))
                              (into {}))]
    (-> node
        (assoc :test updated-test)
        (set-unwind unwind-owned)
        (merge updated-branches))))

(defmethod -memory-management-pass :case
  [node borrowed owned unwind-owned]
  (let [branches (->> node :thens (map-indexed vector) (into {}))
        default (:default node)
        branches (cond-> branches default (assoc :default default))
        owned-in-branch (->> branches
                             (map (fn [[branch-id branch-node]] [branch-id (set/intersection owned (:fresh branch-node))]))
                             (into {}))
        disowned-in-branch (->> branches
                                (map (fn [[branch-id]] [branch-id (set/difference owned (get owned-in-branch branch-id))]))
                                (into {}))
        updated-branches (->> branches
                              (map (fn [[branch-id branch-node]]
                                     [branch-id (-> branch-node
                                                    (-memory-management-pass
                                                     borrowed
                                                     (get owned-in-branch branch-id)
                                                     (set/difference unwind-owned (get disowned-in-branch branch-id)))
                                                    (drop-vars (get disowned-in-branch branch-id)))]))
                              (into {}))
        updated-node (cond-> node default (assoc :default (:default updated-branches)))
        updated-branches (->> (dissoc updated-branches :default) (into []) sort (map second) vec)]
    (-> updated-node
        (set-unwind unwind-owned)
        (assoc :thens updated-branches))))

;; Handled by default:
;; case-then, const, def, import, instance-field, instance?,
;; monitor-enter, monitor-exit, prim-invoke, protocol-invoke, quote, set, static-field, the-var,
;; var, with-meta
(defmethod -memory-management-pass :default
  [node borrowed owned unwind-owned]
  (-> node
      (ast/update-children #(-memory-management-pass % borrowed owned unwind-owned))
      (set-unwind unwind-owned)))

(defn memory-management-pass
  {:pass-info {:walk :none :depends #{#'uniquify-locals #'fresh-vars}}}
  [ast]
  (let [f #(mapv (fn [[k v]] [(name k) v]) %)]
    (postwalk
    ;;  identity
     (fn [m]
       (cond (= (class m) clojure.lang.PersistentTreeMap)
             m
             
             (map? m)
             (reduce (fn [m k] (cond-> m (contains? m k) (update k f)))
                     m
                     [:drop-memory :unwind-memory :all-catches-owned])
             
             :else
             m))
     (-memory-management-pass ast #{} #{} #{}))))


(defn clean-tree
  [node]
  (cond (map? node)
        (if (= (:op node) :case) node
          (let [important-keys (concat (:children node)
                                       [:children :op :loop-let :fresh :form :name :drop-memory
                                        :unwind-memory :local :closed-overs :loops :loop-id :recur-this])]
            (->> (select-keys node important-keys)
                 (map (fn [[k v]] [k (case k
                                       (:drop-memory :unwind-memory :all-catches-owned :loops :loop-id :form) v
                                       :closed-overs (set (keys v))
                                       (clean-tree v))]))
                 (into {}))))
        (vector? node)
        (mapv clean-tree node)
        :else
        node))
