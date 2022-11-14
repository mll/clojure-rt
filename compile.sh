#!/bin/bash

FILENAME="$1"

cp $FILENAME frontend
cd frontend
lein run $FILENAME
mv *.cljb ../backend
rm $FILENAME
cd ../backend
#arch -arm64 lldb ./clojure-rt 
./clojure-rt *.cljb
rm *.cljb
cd ..