(ns clojure.rt.core-test
  (:require [clojure.test :refer :all]
            [clojure.rt.core :refer :all]
            [clojure.tools.analyzer
             [utils :refer [ctx resolve-sym -source-info resolve-ns obj? dissoc-env butlast+last mmerge]]
             [ast :refer [walk prewalk postwalk] :as ast]
             [env :as env :refer [*env*]]
             [passes :refer [schedule]]]))

(deftest ^:test-refresh/focus analyzer
  (testing "Trivial"
    (let [ast (first (analyze "(+ 2 3)" ""))]
      (postwalk ast (fn [node] 
                      (when (map? node)
                        (is (empty? (:unwind-memory node)))
                        (is (empty? (:drop-memory node))))
                      node))))

  (testing "Variable"
    (let [ast (first (analyze "(let [x 3] (+ 2 x))" ""))]
      (postwalk ast (fn [node]
                      (when (map? node)
                        ;; Some nodes might have guidance now, but trivial ones shouldn't have redundant ones
                        (when (= :static-call (:op node))
                          (is (empty? (:unwind-memory node)))))
                      node)))))
