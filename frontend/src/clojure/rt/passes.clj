(ns clojure.rt.passes
  (:refer-clojure :exclude [compile])
  (:require [clojure.set :as set]
            [clojure.tools.analyzer.ast :as ast]
            [clojure.tools.analyzer.passes
             [uniquify :refer [uniquify-locals]]
             [collect-closed-overs :refer [collect-closed-overs]]]
            [clojure.walk :refer [postwalk]]))

(defn mm-pass-one
  ^{:pass-info {:walk :none :depends #{#'uniquify-locals #'collect-closed-overs} :state (fn [] (atom {}))}}
  ([ast] (mm-pass-one (atom {}) ast))
  ([state ast]
   (case (:op ast)
     :binding (swap! state (fn [old-state]
                           (let [name (:name ast)]
                             (if (get old-state name)
                               (update old-state name inc)
                               (assoc old-state name 1)))))
     nil)
  ;;  (println "AAAAAAAA" (:op ast) " - " state)
   (assoc ast :mm-refs @state)))


(defmulti -fresh-vars :op)

(defmethod -fresh-vars :var
  [node]
  (assoc node :fresh #{}))

(defmethod -fresh-vars :local
  [node]
  (assoc node :fresh #{(:name node)}))

(defmethod -fresh-vars :const
  [node]
  (assoc node :fresh #{}))

(defmethod -fresh-vars :fn-method
  [node]
  (let [updated-node (ast/update-children node -fresh-vars)]
    (-> updated-node
        (assoc :fresh (set/difference (:fresh (:body updated-node))
                                      (set (map :name (:params updated-node))))))))

(defmethod -fresh-vars :let
  [node]
  (let [updated-body (-fresh-vars (:body node))
        [reversed-updated-bindings fresh]
        (->> node
             :bindings
             reverse
             (reduce (fn [[new-bindings fresh] binding]
                       (let [updated-init (-fresh-vars (:init binding))
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
  [node]
  (let [updated-ret (-fresh-vars (:ret node))
        [reversed-updated-statements fresh]
        (->> node
             :statements
             reverse
             (reduce (fn [[new-statements fresh] statement]
                       (let [updated-statement (-fresh-vars statement)
                             binding-fresh (set/union fresh (:fresh updated-statement))
                             updated-statement (assoc updated-statement :fresh binding-fresh)]
                         [(conj new-statements updated-statement) binding-fresh]))
                     [[] (:fresh updated-ret)]))
        updated-statements (vec (reverse reversed-updated-statements))]
    (-> node
        (assoc :ret updated-ret
               :statements updated-statements
               :fresh fresh))))

;; Revisit catch and try when implementing exceptions
(defmethod -fresh-vars :catch
  [node]
  (let [updated-body (-fresh-vars (:body node))
        fresh (disj (:fresh updated-body) (:name (:local node)))]
    (-> node
        (assoc :body updated-body
               :fresh fresh))))

(defmethod -fresh-vars :default
  [node]
  (let [updated-node (ast/update-children node -fresh-vars)
        all-fresh (->> updated-node ast/children (map :fresh) (apply set/union))]
    (-> updated-node
        (assoc :fresh all-fresh))))

(defn fresh-vars
  ^{:pass-info {:walk :none :depends #{#'uniquify-locals}}}
  [ast]
  (-fresh-vars ast))


(defmulti -memory-management-pass
  (fn [node borrowed owned unwind-owned]
    (assert (empty? (set/intersection borrowed owned)) "Invariant violation: borrowed and owned intersection is nonempty")
    (assert (set/subset? owned (:fresh node)) "Invariant violation: owned not a subset of fresh variables")
    (assert (set/subset? unwind-owned (set/union borrowed owned)) "Invariant violation: unwind-owned not a subset of owned + borrowed")
    (assert (set/subset? (:fresh node) (set/union borrowed owned)) "Invariant violation: fresh variables not a subset of borrowed and owned")
    (:op node)))

(defn update-drop-memory
  [node name->amount]
  (update node :drop-memory #(merge-with + % name->amount)))

(defn drop-vars
  [node names]
  (update-drop-memory node (->> names (map #(vector % -1)) (into {}))))

(defn dup-vars
  [node names]
  (update-drop-memory node (->> names (map #(vector % 1)) (into {}))))

(defn set-unwind
  [node names]
  (assoc node :unwind-memory (->> names (map #(vector % -1)))))

;; In paper: SVAR, SVAR-DUP

(defn variable-usage
  [node owned name]
  (or (and (= owned #{name}) node)
      (and (empty? owned)
           ;; (borrowed name) ;; disabled due to how defs work
           (dup-vars node [name]))
      (assert false "Invariant violation: unexpected SVAR case\n")))

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
        updated-nodes (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :fn (first updated-nodes)
               :args (vec (rest updated-nodes))))))

(defmethod -memory-management-pass :static-call
  [node borrowed owned unwind-owned]
  (let [nodes (:args node)
        updated-nodes (application-usage nodes borrowed owned unwind-owned)]
    (-> node
        (set-unwind unwind-owned)
        (assoc :args updated-nodes))))

;; In paper: SLAM and SLAM-DROP, merged to modify function with multiple arities

(defn function-definition
  [node _borrowed owned unwind-owned]
  (let [fn-fresh (:fresh node) ;; ys
        args (set (map :name (:params node)))
        used (set/intersection args (:fresh (:body node))) ;; x in fv(e)
        unused (set/difference args used) ;; x not in fv(e)
        duplicated (set/difference fn-fresh owned) ;; Delta_1
        updated-body (-memory-management-pass (:body node) #{} (set/union fn-fresh used) (set/union unwind-owned used))]
    (-> node
        (assoc :body updated-body)
        (update :body drop-vars unused)
        (set-unwind unwind-owned)
        (dup-vars duplicated))))

(defmethod -memory-management-pass :fn-method
  [node borrowed owned unwind-owned]
  (function-definition node borrowed owned unwind-owned))

(defmethod -memory-management-pass :fn
  [node borrowed owned unwind-owned]
  (update node :methods (fn [methods] (mapv #(function-definition % borrowed owned unwind-owned) methods))))

;; In paper: SBIND and SBIND-DROP, adapted to handle multiple bindings in let

(defmethod -memory-management-pass :let
  ;; Earlier pass guarantees that there is (initially) at least one binding
  [node borrowed owned unwind-owned]
  (if (empty? (:bindings node))
    (update node :body -memory-management-pass borrowed owned unwind-owned)
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
          updated-let (assoc node :bindings bindings :fresh remainder-fresh)
          updated-remainder (-memory-management-pass updated-let
                                                     borrowed
                                                     (cond-> remainder-owned local-name-used (conj local-name))
                                                     (cond-> remainder-unwind-owned local-name-used (conj local-name)))
          drop-local-updated-remainder (if local-name-used
                                         updated-remainder
                                         (let [ks (if (seq bindings) [:bindings 0] [:body])]
                                           (update-in updated-remainder ks drop-vars [local-name])))]
      (-> drop-local-updated-remainder
          (assoc :fresh (:fresh node))
          ;; (set-unwind unwind-owned) ;; refer to individual bindings/body
          (update :bindings #(vec (concat [updated-binding] %)))))))

(defmethod -memory-management-pass :do
  ;; Earlier pass guarantees that there is (initially) at least one statement
  [node borrowed owned unwind-owned]
  (if (empty? (:statements node))
    (update node :ret -memory-management-pass borrowed owned unwind-owned)
    (let [[statement & statements] (:statements node)
          remainder-fresh (if (seq statements) (:fresh (first statements)) (:fresh (:ret node))) ;; fv(e_2)
          remainder-owned (set/intersection owned remainder-fresh) ;; Gamma_2
          updated-statement (-memory-management-pass statement
                                                     (set/union borrowed remainder-owned)
                                                     (set/difference owned remainder-owned)
                                                     (set/intersection unwind-owned (set/union borrowed owned)))
          updated-do (assoc node :statements statements :fresh remainder-fresh)
          updated-remainder (-memory-management-pass updated-do
                                                     borrowed
                                                     remainder-owned
                                                     (set/intersection unwind-owned (set/union borrowed remainder-owned)))]
      (-> updated-remainder
          (assoc :fresh (:fresh node))
          ;; (set-unwind unwind-owned) ;; refer to individual statements/ret
          (update :statements #(vec (concat [updated-statement] %)))))))

;; Revisit catch and try when implementing exceptions
#_(defmethod -memory-management-pass :try
    ;; Earlier pass guarantees that there is at least one catch OR finally block
  ;; 1st attempt: without finally
    [node borrowed owned]
    (let []))

#_(defmethod -memory-management-pass :catch
    [node borrowed owned unwind-owned]
    (let [exception-name (:name (:local node))
          remainder-fresh (:fresh (:body node))
          exception-used (remainder-fresh exception-name)
          remainder-owned (set/intersection owned (disj remainder-fresh exception-name))
          remainder-unwind-owned (set/intersection unwind-owned (disj remainder-fresh exception-name))
          updated-body (-memory-management-pass (:body node)
                                                (set/union borrowed remainder-owned)
                                                (set/difference owned remainder-owned)
                                                (set/difference unwind-owned remainder-unwind-owned))
          drop-exception-updated-body (cond-> updated-body (not exception-used) (drop-vars [exception-name]))]
      (-> node
          (assoc :body drop-exception-updated-body)
          (set-unwind unwind-owned))))

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

(defmethod -memory-management-pass :default
  [node borrowed owned unwind-owned]
  (-> node
      (ast/update-children #(-memory-management-pass % borrowed owned unwind-owned))
      (set-unwind unwind-owned)))

(defn memory-management-pass
  ^{:pass-info {:walk :none :depends #{#'uniquify-locals #'fresh-vars}}}
  [ast]
  (let [f #(mapv (fn [[k v]] [(name k) v]) %)]
    (postwalk
    ;;  identity
     (fn [m] (if (map? m)
               (cond-> m
                 (contains? m :drop-memory) (update :drop-memory f)
                 (contains? m :unwind-memory) (update :unwind-memory f))
               m))
     (-memory-management-pass ast #{} #{} #{}))))


(defn clean-tree
  [node]
  (cond (map? node)
        (let [important-keys (concat (:children node)
                                     [:op :fresh :form :name :drop-memory :unwind-memory])]
          (->> (select-keys node important-keys)
               (map (fn [[k v]] [k (if (#{:drop-memory :unwind-memory} k) v (clean-tree v))]))
               (into {})))
        (vector? node)
        (mapv clean-tree node)
        :else
        node))
