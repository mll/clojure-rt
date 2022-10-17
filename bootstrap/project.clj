(defproject clojure-rt-bootstrap "0.1.0-SNAPSHOT"
  :description "FIXME: write description"
  :url "http://example.com/FIXME"
  :license {:name "EPL-2.0 OR GPL-2.0-or-later WITH Classpath-exception-2.0"
            :url "https://www.eclipse.org/legal/epl-2.0/"}
  :dependencies [[org.clojure/clojure "1.10.3"]
                 [org.clojure/tools.reader "1.3.6"]
                 [org.clojure/tools.analyzer.jvm "1.2.2"]
                 [org.clojure/tools.emitter.jvm "0.1.0-beta5"]
                 [camel-snake-kebab "0.4.3"]
                 [clojusc/protobuf "3.5.1-v1.1" :exclusions [com.google.protobuf/protobuf-java]]
                 [com.google.protobuf/protobuf-java "3.21.7"]]
  :repl-options {:init-ns clojure-rt-bootstrap.core}
  :java-source-paths ["protobuf"])
