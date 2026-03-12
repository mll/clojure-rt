;; Currently this collapses (unable to generate dynamic call) and this is good
;; because dynamic calls to equiv are not yet implemented. 
;; What is not good is that var leaks from the test. 
;; It is not yet clear why, but there is an exception being thrown in code generation, maybe that's the reason?
;; Also - threadsafe registry always returns stuff at +1, so there should be a release if we cannot proceed?

(def a 5)
(if (= a 5) "zulu" "gula")