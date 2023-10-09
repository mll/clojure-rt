(defproject clojure.rt "1.0.0"
  :description "FIXME: write description"
  :url "http://example.com/FIXME"
  :license {:name "EPL-2.0 OR GPL-2.0-or-later WITH Classpath-exception-2.0"
            :url "https://www.eclipse.org/legal/epl-2.0/"}
  :dependencies [[org.clojure/clojure "1.11.1"]
                 [org.clojure/tools.reader "1.3.6"]
                 [org.clojure/tools.analyzer.jvm "1.2.2"]
                 [org.clojure/tools.emitter.jvm "0.1.0-beta5"]
                 [camel-snake-kebab "0.4.3"]
                 [io.github.protojure/core "2.0.1"]
                 [io.github.protojure/google.protobuf "2.0.0"]]
  :repl-options {:init-ns clojure.rt.core}
  :main clojure.rt.core
  :java-source-paths []
  :profiles
  {:uberjar {:omit-source true
             :aot :all
             :jvm-opts ["-Xmx800m"]
             :uberjar-name "clojure-rt.jar"
             :source-paths ["env/prod/clj"]
             :resource-paths ["env/prod/resources"]
             :target-path "target/uberjar"}

   :dev           [:project/dev :profiles/dev]
   :test          [:project/dev :project/test :profiles/test]

   :project/dev  {:jvm-opts ["-Dconf=dev-config.edn"]
                  :dependencies [[expound "0.8.5"]
                                 [pjstadig/humane-test-output "0.10.0"]
                                 [prone "2020-01-17"]
                                 [com.taoensso/tufte "2.1.0"]
                                 [org.clojure/clojure "1.11.1"]
                                 [ring/ring-devel "1.9.5"]
                                 [ring/ring-mock "0.4.0"]
                                 [org.clojure/test.check "1.1.0"]]
                  :plugins      [[com.jakemccrary/lein-test-refresh "0.24.1"]]

                  :source-paths ["env/dev/clj"]
                  :resource-paths ["env/dev/resources"]
                  :repl-options {:init-ns user}
                  :injections [(require 'pjstadig.humane-test-output)
                               (pjstadig.humane-test-output/activate!)]}
   :project/test {:dependencies []
                  :jvm-opts ["-Dconf=test-config.edn" "-XX:-OmitStackTraceInFastThrow"]
                  :resource-paths ["env/test/resources"]}
   :profiles/dev {}
   :profiles/test {}}
  
  )
