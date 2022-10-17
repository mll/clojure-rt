
(ns clojure-rt-bootstrap.protobuf
  (:require [clojure.tools.reader.edn :as edn]
            [camel-snake-kebab.core :as csk]
            [clojure.string :refer [join trim lower-case] :as s]
            [protobuf.core :as pb]))

(defn- capitalize [in] (str (apply str (concat [(first (s/capitalize in))] (rest in)))))
(defn- cl->pt [s] (let [n (name s)] (csk/->camelCase 
                                    (case (last n) 
                                      \? (str "is-" (apply str (drop-last n))) 
                                      \! (str "mutate-" (apply str (drop-last n)))
                                      n))))

(def all-keys-types {:op :op
                     :form :string
                     :env :string
                     :raw-forms [:string] 
                     :top-level :bool 
                     :tag :string
                     :o-tag :string
                     :ignore-tag :bool 
                     :loops [:string]
                     :subnode :subnode})


(defn- import-by-name [n]
  (.importClass (the-ns *ns*)
                (clojure.lang.RT/classForName n)))

(defn- class-name-for-op [op]
  (str (str "clojureRT.protobuf.Protobuf$" (-> op cl->pt capitalize) "Node") ))

(defn- class-for-op [op] (resolve (symbol (class-name-for-op op))))
(def class-for-subnode clojureRT.protobuf.Protobuf$Subnode)


(defn- import-all [] (doseq [n (->> "resources/ast-ref.edn" 
                                  (slurp)
                                  (edn/read-string)
                                  :node-keys
                                  (map :op))]
                     (let [nvec (class-name-for-op n)]  
                       (import-by-name nvec))))

(import-all)


(defn- encode-node [node-key-types n]
  (let [node (assoc n :subnode n)
        op (:op node)
        node-type (op node-key-types)
        node-class (class-for-op op)
        node-superclass (class-for-op "")
        proto-symbol (comp keyword lower-case csk/->camelCase name)

        converters {:string str
                    :int identity
                    :bool identity
                    :node (partial encode-node node-key-types)}

        map-node (fn [keymap converters] 
                   (into {} 
                         (filter identity 
                                 (map (fn [[k t]]
                                        (let [val (k node)
                                              repeated? (sequential? t)
                                              type (if repeated? (first t) t)
                                              ;; no converter means enum
                                              converter (or (type converters) 
                                                            #(keyword (str (name (proto-symbol type)) (name (proto-symbol %)))))]
                                          (if val
                                            [(proto-symbol k) 
                                             (if repeated? (map converter val) (converter val))]))) keymap))))
        create-subnode (fn [_] 
                         (protobuf/create 
                          class-for-subnode 
                          {op 
                           (protobuf/create node-class (map-node node-type converters))}))]
    (protobuf/create node-superclass 
                     (map-node all-keys-types 
                               (assoc converters 
                                      :subnode create-subnode)))))
(defn encode-ast [ast]
  (encode-node (-> "resources/ast-types.edn" (slurp) (edn/read-string)) ast))


(defn generate-protobuf-defs [outfile]
  (let [data (-> "resources/ast-ref.edn" 
                 (slurp)
                 (edn/read-string))
        local-type [:arg :catch :fn :let :letfn :loop :field :this]
        
        gen-subnode #(str "  " (capitalize (cl->pt %)) "Node " (cl->pt %))
        
        header (str "syntax = \"proto3\";\n\npackage clojure;\noption java_outer_classname = \"Protobuf\";\n\nenum Op {\n" 
                    (join "\n"
                     (->> data 
                          :node-keys
                          (map :op)
                          (map cl->pt)
                          (map capitalize)
                          (map-indexed #(str "  op" %2 " = " %1 ";"))))
                    "\n}\n\n"
                    "enum LocalType {\n"
                    (join "\n"
                          (->> local-type 
                               (map cl->pt)
                               (map capitalize)
                               (map-indexed #(str "  localType" %2 " = " %1 ";"))))
                    "\n}\n\n"
                    "message Subnode {\n  oneof types {\n"
                    (join "\n" 
                          (->> data 
                          :node-keys
                          (map :op)
                          (map gen-subnode)
                          (map-indexed #(str "  " %2 " = " (inc %1) ";"))))
                    "\n  }\n}\n\n")
        
        types {:string "string"
               :int "uint32"
               :bool "bool"
               :node "Node"}
        
        node-keys-types (-> "resources/ast-types.edn" (slurp) (edn/read-string))
        gentype (fn [[op t]]
                  (let [ast (if (= op :node) {:keys (:all-keys data)
                                              :op :node
                                              :doc "Universal node containing common fields"} 
                                (first (filter #(= op (:op %)) (:node-keys data))))
                        start (str "/* " (:doc ast) " */\n" "message " 
                                   (capitalize (cl->pt op)) 
                                   (if (not= op :node) "Node" "") " {\n")
                        enums (map (fn [[field desc]] 
                                       (str "  enum " (capitalize (cl->pt field)) " {\n"
                                            (join "\n"
                                                  (->> desc
                                                       (map cl->pt)
                                                       (map capitalize)
                                                       (map-indexed #(str "    " (cl->pt field) %2 " = " %1  ";"))))
                                            "\n  }\n")) 
                                     (:enums t)) 
                        fields (map (fn [[field desc]]
                                      (let [field-ast (first (filter #(= field (first %)) (:keys ast)))
                                            repeated?  (sequential? desc)
                                            type-key (if repeated? (first desc) desc)
                                            type (or (type-key types)
                                                     (capitalize (cl->pt type-key)))
                                            optional? (and (not repeated?) (:optional (meta field-ast)))]
                                        (assert (or (= field :subnode) field-ast) 
                                                (str "Field " field  " not found in ast description of message " op))
                                        (str (if (and field-ast (second field-ast) (seq (trim (second field-ast)))) (str "/* " (second field-ast) " */\n  ") "")
                                             (if optional? "optional " "")
                                             (if repeated? "repeated " "")
                                             type " "
                                             (cl->pt field)))) (sort-by first (dissoc t :enums)))]
                    (str start 
                         (join "\n" enums) (if (seq enums) "\n" "") 
                         (join "\n\n" (map-indexed #(str "  " %2 " = " (inc %1) ";") fields))
                         "\n}")))]
    (spit outfile (str header (join "\n\n" (map gentype (sort-by first node-keys-types))) "\n\n"
                       (gentype [:node all-keys-types])))))
