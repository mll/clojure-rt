(ns clojure.rt.protobuf.schema
  (:require [clojure.tools.reader.edn :as edn]
            [camel-snake-kebab.core :as csk]
            [clojure.string :refer [join trim lower-case] :as s]
            [clojure.java.io :as io]))

(defn- read-file [f] (slurp (-> f io/resource io/file)))
(defn- capitalize [in] (str (apply str (concat [(first (s/capitalize in))] (rest in)))))
(defn- cl->pt [s] (let [n (name s)] (csk/->camelCase 
                                    (case (last n) 
                                      \? (str "is-" (apply str (drop-last n))) 
                                      \! (str "mutate-" (apply str (drop-last n)))
                                      n))))

(def types {:string "string"
            :int "uint32"
            :bool "bool"
            :node "Node"
            :environment "Environment"})


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


(defn generate-protobuf-defs [outfile]
  (let [data (-> "ast-ref.edn" 
                 (read-file)
                 (edn/read-string))
        local-type [:arg :catch :fn :let :letfn :loop :field :this]
        
        gen-subnode #(str "  " (capitalize (cl->pt %)) "Node " (cl->pt %))
        
        header (str "syntax = \"proto3\";\n\npackage clojure.rt.protobuf.bytecode;\n\nenum Op {\n" 
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
                    "message Environment {\n"
                    (join "\n"
                          (->> environment-keys
                               (map (fn [[k v]]
                                      (let [type ((if (sequential? v) (first v) v) types)]
                                        (str "  " 
                                             (if (sequential? v) "repeated " "")
                                             type " " (cl->pt k)))))
                               (map-indexed #(str  %2 " = " (inc %1) ";"))))
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
        
        node-keys-types (-> "ast-types.edn" (read-file) (edn/read-string))
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
                                        (str (if (and field-ast (second field-ast) (seq (trim (second field-ast)))) (str "/* " (if optional? "OPTIONAL: " "") (second field-ast) " */\n  ") "")
                                             (if repeated? "repeated " "")
                                             type " "
                                             (cl->pt field)))) (sort-by first (dissoc t :enums)))]
                    (str start 
                         (join "\n" enums) (if (seq enums) "\n" "") 
                         (join "\n\n" (map-indexed #(str "  " %2 " = " (inc %1) ";") fields))
                         "\n}")))]
    (spit outfile (str header 
                       (join "\n\n" (map gentype (sort-by first node-keys-types))) "\n\n"
                       (gentype [:node all-keys-types]) "\n\n"
                       "message Programme {\n  repeated Node nodes = 1;\n}\n"
                       ))))

