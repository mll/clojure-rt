(ns clojure.rt.protobuf.encoder
  (:require [clojure.tools.reader.edn :as edn]
            [camel-snake-kebab.core :as csk]
            [clojure.string :refer [join trim lower-case] :as s]
            [clojure.rt.protobuf.bytecode :as bc]
            [clojure.java.io :as io]))

(defn- read-file [f] (slurp (-> f io/resource io/file)))

(defn- capitalize [in] (str (apply str (concat [(first (s/capitalize in))] (rest in)))))
(defn- cl->pt [s] (let [n (name s)] (csk/->camelCase 
                                    (case (last n) 
                                      \? (str "is-" (apply str (drop-last n))) 
                                      \! (str "mutate-" (apply str (drop-last n)))
                                      n))))

(def all-keys-types {:op :op
                     :form :string
                     :env :environment
                     :raw-forms [:string] 
                     :top-level :bool 
                     :tag :string
                     :o-tag :string
                     :ignore-tag :bool 
                     :loops [:string]
                     :subnode :subnode})

(def environment-keys {:context :string
                       :locals [:string]
                       :ns :string
                       :column :int
                       :line :int
                       :end-column :int
                       :end-line :int
                       :file :string})


(defn- ctor-for-op [op] 
  (let [ctor-name (str "clojure.rt.protobuf.bytecode/new-" (capitalize (cl->pt op)) "Node")
        ctor (resolve (symbol ctor-name))]
    (assert ctor (str "No ctor for op: " op ", " ctor-name))
    ctor))


(defn- encode-node [node-key-types n]
  (let [node (assoc n :subnode n)
        op (:op node)
        node-type (op node-key-types)
        proto-symbol (comp keyword lower-case cl->pt)

        converters {:string str
                    :int identity
                    :bool identity
                    :node (partial encode-node node-key-types)}

        map-node (fn [keymap converters in-node] 
                   (into {} 
                         (filter identity 
                                 (map (fn [[k t]]
                                        (let [val (k in-node)
                                              repeated? (sequential? t)
                                              type (if repeated? (first t) t)
                                              ;; no converter means enum
                                              converter (or (type converters) 
                                                            #(keyword 
                                                              (lower-case 
                                                               (str (name (proto-symbol type))  
                                                                    (name (proto-symbol %))))))]
                                          (if val
                                            [(proto-symbol k) 
                                             (if repeated? (vec (map converter val)) (converter val))]))) keymap))))
        create-subnode (fn [subnode] 
                         (bc/new-Subnode  
                          {:types {(keyword (cl->pt op)) ((ctor-for-op op) (map-node node-type converters subnode))}}))
        create-environment (fn [environ]
                             (bc/new-Environment (map-node environment-keys converters environ)))]
    (bc/new-Node (map-node all-keys-types 
                           (assoc converters 
                                  :subnode create-subnode
                                  :environment create-environment) node))))
(defn encode-ast [programme]
  (let [types (-> "ast-types.edn" (read-file) (edn/read-string))]
    (bc/new-Programme {:nodes (map #(encode-node types %) programme)})))


