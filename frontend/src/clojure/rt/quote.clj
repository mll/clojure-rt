(ns clojure.rt.quote
  (:require [clojure.tools.analyzer :refer [analyze-const analyze-in-env]]
            [clojure.tools.analyzer.utils :refer [-source-info classify ctx]]))

(defn deep-quote
  [form]
  (case (classify form)
    :map    (->> form (map (fn [[k v]] [(deep-quote k) (deep-quote v)])) (into {}))
    :vector (mapv deep-quote form)
    :set    (set (map deep-quote form))
    :seq    (sequence (map deep-quote form))
            (list 'quote form)))

(defn map-of-quotes
  [env]
  (fn [form]
    (let [kv-env (ctx env :ctx/expr)
          [keys vals] (reduce-kv (fn [[keys vals] k v]
                                   [(conj keys k) (conj vals v)])
                                 [[] []] form)
          ks (mapv (comp (analyze-in-env kv-env) #(list 'quote %)) keys)
          vs (mapv (comp (analyze-in-env kv-env) #(list 'quote %)) vals)]
      {:op       :map
       :env      env
       :keys     ks
       :vals     vs
       :form     form
       :children [:keys :vals]})))

(defn vector-of-quotes
  [env]
  (fn [form]
    (let [items-env (ctx env :ctx/expr)
          items (mapv (comp (analyze-in-env items-env) #(list 'quote %)) form)]
      {:op       :vector
       :env      env
       :items    items
       :form     form
       :children [:items]})))

(defn set-of-quotes
  [env]
  (fn [form]
    (let [items-env (ctx env :ctx/expr)
          items (mapv (comp (analyze-in-env items-env) #(list 'quote %)) form)]
      {:op       :set
       :env      env
       :items    items
       :form     form
       :children [:items]})))

(defn seq-of-quotes
  [env]
  (fn [form]
    (let [items-env (ctx env :ctx/expr)
          items (mapv (comp (analyze-in-env items-env) #(list 'quote %)) form)]
      {:op   :invoke
       :form `(sequence (list ~@form))
       :env  env
       :fn   {:op :var
              :var #'clojure.core/sequence
              :form 'clojure.core/sequence}
       :args [{:op   :invoke
               :form `(list ~@form)
               :env  env
               :fn   {:op :var
                      :var #'clojure.core/list
                      :form 'clojure.core/list}
               :args items
               :children [:fn :args]}]
       :children [:fn :args]})))

(defn parse-quote
  [[_ expr :as form] env]
  (when-not (= 2 (count form))
    (throw (ex-info (str "Wrong number of args to quote, had: " (dec (count form)))
                    (merge {:form form}
                           (-source-info form env)))))
  (let [expr-type (classify expr) 
        base-types #{:nil :bool :keyword :symbol :string :number :type :char :regex :var}
        composite-types {:map (map-of-quotes env)
                         :vector (vector-of-quotes env)
                         :set (set-of-quotes env)
                         :seq (seq-of-quotes env)}
        _unknown-types #{:unknown :record :class}]
    (if (base-types expr-type)
      (let [const (analyze-const expr env)]
        {:op       :quote
         :expr     const
         :form     form
         :env      env
         :literal? true
         :children [:expr]})
      (if-let [transform-form (composite-types expr-type)]
        (transform-form expr)
        (throw (Exception. (str "parse-quote: unimplemented " expr-type)))))))
