(ns clojure.rt.passes-test
  (:require [clojure.test :refer :all]
            [clojure.rt.core :as core]
            [clojure.rt.passes :as passes]
            [clojure.tools.analyzer.jvm :as a]
            [clojure.tools.analyzer.ast :refer [postwalk]]))

(deftest expand-const-collections-test
  (testing "Map expansion"
    (let [env (a/empty-env)
          ast {:op :const :val {:a 1} :type :map :env env}
          expanded (passes/expand-const-collections ast)]
      (is (= :map (:op expanded)))
      (is (= 1 (count (:keys expanded))))
      (is (= :const (:op (first (:keys expanded)))))
      (is (= :const (:op (first (:vals expanded)))))))

  (testing "Vector expansion"
    (let [env (a/empty-env)
          ast {:op :const :val [1 2] :type :vector :env env}
          expanded (passes/expand-const-collections ast)]
      (is (= :vector (:op expanded)))
      (is (= 2 (count (:items expanded))))))

  (testing "Recursive expansion"
    (let [env (a/empty-env)
          ast {:op :const :val {:a [1]} :type :map :env env}
          expanded (passes/expand-const-collections ast)]
      (is (= :map (:op expanded)))
      (let [val-node (first (:vals expanded))]
        (is (= :vector (:op val-node)))
        (is (= 1 (count (:items val-node)))))))

  (testing "Sequence expansion"
    (let [env (a/empty-env)
          ast {:op :const :val (seq [1 2]) :type :seq :env env}
          expanded (passes/expand-const-collections ast)]
      (is (= :invoke (:op expanded)))
      (is (= 'clojure.core/sequence (:form (:fn expanded))))))

  (testing "Metadata preservation on collections"
    (let [env (a/empty-env)
          meta-ast {:op :const :val {:foo true} :type :map :env env}
          ast {:op :const :val [1 2] :type :vector :env env :meta meta-ast}
          expanded (passes/expand-const-collections ast)]
      (is (= :vector (:op expanded)))
      (is (= meta-ast (:meta expanded)) "Metadata should be preserved on expanded collection")))

  (testing "Metadata preservation on lists (seq)"
    (let [env (a/empty-env)
          meta-ast {:op :const :val {:foo true} :type :map :env env}
          ast {:op :const :val '(1 2) :type :seq :env env :meta meta-ast}
          expanded (passes/expand-const-collections ast)]
      (is (= :with-meta (:op expanded)))
      (is (= meta-ast (:meta expanded)) "Metadata should be preserved on expanded sequence")))

  (testing "Integration with analyze"
    (let [ast (core/analyze "'{:a 1}" "test.clj")
          const-maps (atom [])]
      (postwalk ast (fn [node]
                      (when (and (map? node) 
                                 (= :const (:op node)) 
                                 (= :map (:type node)))
                        (swap! const-maps conj node))
                      node))
      (is (empty? @const-maps) "No constant map nodes should remain after expansion"))))
