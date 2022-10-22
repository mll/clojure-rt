#!/bin/bash

FILENAME="$1"
echo "$FILENAME"

cp $1 frontend
cd frontend
lein run $1
mv *.cljb ../backend
cd ../backend
#arch -arm64 lldb ./clojure-rt 
./clojure-rt *.cljb
rm *.cljb
cd ..