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
      (postwalk ast #(is (= {} (:mm-refs %))))))

  (testing "Variable"
    (let [ast (first (analyze "(let [x 3] (+ 2 x))" ""))]
      (postwalk ast #(is (= {} (:mm-refs %)) %)))))
