(defproject clojure.rt "1.0.0"
  :description "FIXME: write description"
  :url "http://example.com/FIXME"
  :license {:name "EPL-2.0 OR GPL-2.0-or-later WITH Classpath-exception-2.0"
            :url "https://www.eclipse.org/legal/epl-2.0/"}
  :dependencies [[org.clojure/clojure "1.10.3"]
                 [org.clojure/tools.reader "1.3.6"]
                 [org.clojure/tools.analyzer.jvm "1.2.2"]
                 [org.clojure/tools.emitter.jvm "0.1.0-beta5"]
                 [camel-snake-kebab "0.4.3"]
                 [io.github.protojure/core "2.0.1"]
                 [io.github.protojure/google.protobuf "2.0.0"]]
  :repl-options {:init-ns clojure.rt.core}
  :main clojure.rt.core
  :java-source-paths [])
