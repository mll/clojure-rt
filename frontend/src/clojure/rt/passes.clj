(ns clojure.rt.passes
  (:refer-clojure :exclude [compile])
  (:require [clojure.tools.reader :as r]
            [clojure.tools.analyzer.jvm :as a]
            [clojure.tools.emitter.jvm :as e]
            
            [clojure.tools.analyzer
             [utils :refer [ctx resolve-sym -source-info resolve-ns obj? dissoc-env butlast+last mmerge]]
             [ast :refer [walk prewalk postwalk] :as ast]
             [env :as env :refer [*env*]]
             [passes :refer [schedule]]]

            [clojure.tools.analyzer.jvm.utils :refer :all :as u :exclude [box specials]]
            [clojure.tools.analyzer.passes
             [uniquify :refer [uniquify-locals]]
             [collect-closed-overs :refer [collect-closed-overs]]]))

(defn mm-pass-one  
  ^{:pass-info {:walk :none :depends #{#'uniquify-locals #'collect-closed-overs} :state (fn [] (atom {}))}}
  ([ast] (mm-pass-one (atom {}) ast))
  ([state ast]
   (case (:op ast)
     :local (swap! state (fn [old-state] 
                           (let [name (:name ast)]
                             (if (get old-state name)
                               (update old-state name inc)
                               (assoc old-state name 1)))))
     nil)
;   (println "AAAAAAAA" (:op ast) " - " state)
   (assoc ast :mm-refs @state)))
   
   
