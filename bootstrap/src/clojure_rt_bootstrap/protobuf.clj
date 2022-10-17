(ns clojure-rt-bootstrap.protobuf
  (:require [clojure.tools.reader.edn :as edn]
            [camel-snake-kebab.core :as csk]
            [clojure.string :refer [join trim] :as s]
            [protobuf.core :as pb])
  (:import [clojureRT.protobuf Protobuf$ConstNode]))





(defn generate-protobuf-defs [outfile]
  (let [capitalize #(str (apply str (concat [(first (s/capitalize %))] (rest %))))
        cl->pt (fn [s] (let [n (name s)] (csk/->camelCase 
                                          (case (last n) 
                                            \? (str "is-" (apply str (drop-last n))) 
                                            \! (str "mutate-" (apply str (drop-last n)))
                                            n))))
        data (-> "resources/ast-ref.edn" 
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
        all-keys-types {:op :op
                        :form :string
                        :env :string
                        :raw-forms [:string] 
                        :top-level :bool 
                        :tag :string
                        :o-tag :string
                        :ignore-tag :bool 
                        :loops [:string]
                        :subnode :subnode} 
        
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
