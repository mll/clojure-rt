(ns clojure.rt.hoist-test
  (:require [clojure.test :refer :all]
            [clojure.rt.core :as core]
            [clojure.rt.passes :as passes]
            [clojure.tools.analyzer.ast :as ast]))

(deftest test-hoist-transformation
  (testing "Invoke with expression fn is hoisted to let"
    (let [code "((fn [] 1N) 2N)"
          ast (first (core/analyze code "test.clj"))
          hoisted (passes/hoist-invoke-fns ast)]
      (is (= :let (:op hoisted)))
      (let [binding (first (:bindings hoisted))
            body (:body hoisted)]
        (is (= :invoke (:op body)))
        (is (= :local (:op (:fn body))))
        (is (= (:name binding) (:name (:fn body)))))))
  
  (testing "Invoke with local fn is NOT hoisted"
    (let [code "(let [f (fn [] 1N)] (f 2N))"
          ast (first (core/analyze code "test.clj"))
          let-body (:body ast)
          invoke-node (if (= :do (:op let-body)) (:ret let-body) let-body)
          hoisted (passes/hoist-invoke-fns invoke-node)]
      (is (= :invoke (:op hoisted)))
      (is (= :local (:op (:fn hoisted)))))))

(run-tests)
