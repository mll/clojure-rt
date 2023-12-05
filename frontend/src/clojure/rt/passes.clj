(ns clojure.rt.passes
  (:refer-clojure :exclude [compile])
  (:require [clojure.set :as set]
            [clojure.tools.analyzer.ast :refer [update-children]]
            [clojure.tools.analyzer.passes
             [uniquify :refer [uniquify-locals]]
             [collect-closed-overs :refer [collect-closed-overs]]]))

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

(defmulti -fresh-and-let-bound-vars (fn [node _ _] (:op node)))

(defn binding-list-fresh-and-let-bound-vars
  [bindings-list fresh let-bound]
  (reduce (fn [[new-bindings bindings-fresh let-bound] binding]
            (let [new-binding (-fresh-and-let-bound-vars binding bindings-fresh let-bound)]
              [(conj new-bindings new-binding)
               (set/difference (set/union bindings-fresh (:fresh new-binding)) (:new-bound-vars new-binding))
               (set/union let-bound (:new-bound-vars new-binding))]))
          [[] fresh let-bound]
          bindings-list))

(defmethod -fresh-and-let-bound-vars :fn-method
  [node fresh let-bound]
  (let [[new-params bindings-fresh bindings-let-bound]
        (binding-list-fresh-and-let-bound-vars (:params node) fresh let-bound)]
    (-> node
        (update :body -fresh-and-let-bound-vars bindings-fresh bindings-let-bound)
        (assoc :params new-params
               :fresh bindings-fresh
               :let-bound bindings-let-bound))))

(defmethod -fresh-and-let-bound-vars :binding
  [node fresh let-bound]
  (let [new-bound-vars (set [(:name node)]) ;; TODO: destructuring, this is only the simplest case
        ;; init exists when variable is introduced by let,
        ;; but not when it is a parameter of a function
        new-init (some-> node :init (-fresh-and-let-bound-vars fresh let-bound)) ;; TODO: analyze init
        value-fresh (:fresh new-init)]
    (cond-> node
      new-init (assoc :init new-init)
      true     (assoc :fresh (set/union fresh value-fresh)
                      :let-bound let-bound
                      :new-bound-vars new-bound-vars))))

(defmethod -fresh-and-let-bound-vars :let
  [node fresh let-bound]
  (let [[new-bindings bindings-fresh bindings-let-bound]
        (binding-list-fresh-and-let-bound-vars (:bindings node) fresh let-bound)]
    (-> node
        (update :body -fresh-and-let-bound-vars bindings-fresh (set/union let-bound bindings-let-bound))
        (assoc :bindings new-bindings
               :fresh bindings-fresh
               :let-bound let-bound))))

(defmethod -fresh-and-let-bound-vars :default
  [node fresh let-bound]
  (-> node
      (assoc :fresh fresh :let-bound let-bound)
      (update-children #(-fresh-and-let-bound-vars % fresh let-bound))))

(defn fresh-and-let-bound-vars
  ^{:pass-info {:walk :none :depends #{#'uniquify-locals}}}
  [ast]
  (-fresh-and-let-bound-vars ast #{} #{}))

(defmulti -memory-management-pass (fn [node _ _] (:op node)))

(defmethod -memory-management-pass :binding
  [_ _ _])

(defmethod -memory-management-pass :default [node _ _] node)

(defn memory-management-pass
  ^{:pass-info {:walk :none :depends #{#'uniquify-locals #'fresh-and-let-bound-vars}}}
  [ast]
  (-memory-management-pass ast #{} #{}))

(defn clean-tree
  [node]
  (cond (map? node)
        (let [important-keys (concat (:children node)
                                     [:op :fresh :let-bound :form])]
          (->> (select-keys node important-keys)
               (map (fn [[k v]] [k (clean-tree v)]))
               (into {})))
        (vector? node)
        (mapv clean-tree node)
        :else
        node))
