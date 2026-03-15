(ns clojure.rt.memory-management-test
  (:require [clojure.test :refer :all]
            [clojure.rt.core :refer :all]
            [clojure.tools.analyzer.ast :as ast]
            [clojure.set :as set]))

(defn- find-node [ast op]
  (first (filter #(= op (:op %)) (ast/nodes ast))))

(defn- get-unwind-names [node]
  (set (map first (:unwind-memory node))))

(defn- get-drop-names [node]
  (set (map first (:drop-memory node))))

(deftest unused-locals-test
  (testing "Unused locals should be dropped"
    ;; (identity x) as a statement in a :do prevents trimming of the let but doesn't use x if we use another literal
    ;; Actually, let's just use a static call that we know won't be trimmed easily.
    (let [ast (first (analyze "(let [x 1N] (. clojure.lang.Numbers (add 2N 3N)))" "test.clj"))
          let-node ast
          body (:body let-node)]
      ;; x is not used in body, so it should be dropped in the body node
      (is (some #(clojure.string/starts-with? % "x") (get-drop-names body))
          (str "Body should drop unused x: " (get-drop-names body))))))

(deftest consumption-unwind-test
  (testing "Consumed arguments should not be in unwind-memory of the consuming node"
    (let [ast (first (analyze "(let [x 1N] (+ x 2N))" "test.clj"))
          call-node (find-node ast :static-call)]
      (is (= :static-call (:op call-node)))
      (let [unwind-names (get-unwind-names call-node)]
        (is (not (some #(clojure.string/starts-with? % "x") unwind-names))
            (str "Unwind memory should not contain consumed x: " unwind-names))))))

(deftest sequential-ownership-test
  (testing "First use in a :do block should consume ownership if available"
    (let [ast (first (analyze "(let [x 1N] (do (+ x 1N) (+ x 2N)))" "test.clj"))
          do-node (find-node ast :do)
          first-statement (first (:statements do-node))
          second-statement (:ret do-node)]
      ;; Under current 'first-wins' logic:
      ;; (+ x 1N) gets ownership and consumes x.
      ;; (+ x 2N) will have to dup x.
      ;; Let's verify this.
      (is (empty? (get-drop-names first-statement)) "First use shouldn't drop (it consumes)")
      ;; We don't easily see dups in the AST yet as it's handled by 'variable-usage' emitting 'dup-vars'
      ;; but we can check that it's NOT dropped at the end of the do block because it was consumed early.
      (is (not (some #(clojure.string/starts-with? % "x") (get-drop-names second-statement)))))))

(deftest branched-drop-test
  (testing "Variables should be dropped in branches where they are no longer used"
    (let [ast (first (analyze "(let [x 1N] (if (. clojure.lang.Numbers (lt 1N 2N)) 2N x))" "test.clj"))
          if-node (find-node ast :if)
          then-branch (:then if-node)
          else-branch (:else if-node)]
      ;; In 'then' branch, x is NOT used, so it should be dropped
      (is (some #(clojure.string/starts-with? % "x") (get-drop-names then-branch))
          (str "Then branch should drop unused x: " (get-drop-names then-branch)))
      ;; In 'else' branch, x IS used (returned), so it should NOT be dropped (it's consumed/moved)
      (is (not (some #(clojure.string/starts-with? % "x") (get-drop-names else-branch)))))))
