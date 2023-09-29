#!/bin/sh

cd ../frontend
lein run
mv bytecode.proto ../model
cd ../model

protoc --cpp_out="../backend/protobuf" bytecode.proto
protoc --clojure_out="../frontend/src" bytecode.proto