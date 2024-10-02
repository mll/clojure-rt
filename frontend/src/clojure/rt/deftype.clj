(ns clojure.rt.deftype
  (:require [clojure.tools.analyzer.jvm :refer [analyze-method-impls
                                                parse-opts+methods]]
            [clojure.tools.analyzer.jvm.utils
             :refer [maybe-class members name-matches? try-best-match]
             :as u]
            
            [clojure.tools.analyzer.utils :refer [dissoc-env source-info]]
            
            [clojure.tools.analyzer :as ana]
            [clojure.tools.analyzer.ast :refer [prewalk]]
            [clojure.tools.analyzer.passes
             [cleanup :refer [cleanup]]
             [elide-meta :refer [elide-meta]]]))

(defn parse-deftype*
  [[_ name class-name fields _ interfaces & methods :as form] env]
  (let [interfaces (disj (set (mapv maybe-class interfaces)) Object)
        this-methods (:methods (meta name))
        superclass (if (contains? (meta name) :superclass)
                     (:superclass (meta name)) ;; might be nil
                     "java.lang.Object")
        fields-expr (mapv (fn [name]
                            {:env     env
                             :form    name
                             :name    name
                             :mutable (let [m (meta name)]
                                        (or (and (:unsynchronized-mutable m)
                                                 :unsynchronized-mutable)
                                            (and (:volatile-mutable m)
                                                 :volatile-mutable)))
                             :local   :field
                             :op      :binding})
                          fields)
        menv (assoc env
                    :context :ctx/expr
                    :locals  (zipmap fields (map dissoc-env fields-expr))
                    :this    class-name)
        [opts methods] (parse-opts+methods methods)
        methods (mapv #(assoc (analyze-method-impls % menv) :interfaces interfaces)
                      methods)
        this-methods (mapv #(assoc (analyze-method-impls % menv) :interfaces interfaces)
                           this-methods)]

    #_(-deftype name class-name fields interfaces)

    {:op         :deftype
     :env        env
     :form       form
     :name       name
     :class-name class-name ;; internal, don't use as a Class
     :fields     fields-expr
     :methods    (into methods this-methods)
     :interfaces interfaces
     :superclass superclass
     :children   [:fields :methods]}))

(defn annotate-host-info
  "Adds a :methods key to reify/deftype :methods info representing
   the reflected informations for the required methods, replaces
   (catch :default ..) forms with (catch Throwable ..)"
  {:pass-info {:walk :pre :depends #{} :after #{#'elide-meta}}}
  [{:keys [op methods interfaces class env] :as ast}]
  (case op
    (:reify :deftype)
    (let [all-methods
          (into #{}
                (mapcat (fn [class]
                          (mapv (fn [method]
                                  (dissoc method :exception-types))
                                (filter (fn [{:keys [flags return-type]}]
                                          (and return-type (not-any? #{:final :static} flags)))
                                        (members class))))
                        (conj interfaces Object)))]
      (assoc ast :methods (mapv (fn [ast]
                                  (let [name (:name ast)
                                        argc (count (:params ast))]
                                    (assoc ast :methods
                                           (filter #(and ((name-matches? name) (:name %))
                                                         (= argc (count (:parameter-types %))))
                                                   all-methods)))) methods)))


    :catch
    (let [the-class (cond

                      (and (= :const (:op class))
                           (= :default (:form class)))
                      Throwable

                      (= :maybe-class (:op class))
                      (u/maybe-class-literal (:class class)))

          ast (if the-class
                (-> ast
                    (assoc :class (assoc (ana/analyze-const the-class env :class)
                                         :form  (:form class)
                                         :tag   Class
                                         :o-tag Class)))
                ast)]
      (assoc-in ast [:local :tag]  (-> ast :class :val)))
    ast))
