#!/bin/bash

LEIN=`which lein`
FILENAME=$1
BINARY="${FILENAME%.clj}.cljb"
cp $FILENAME frontend
cd frontend
echo "Compiling to AST..." 
time $LEIN run $FILENAME
mv $BINARY ../backend
rm $FILENAME
cd ../backend
#arch -arm64 lldb ./clojure-rt 
echo "Executing..."
./clojure-rt $BINARY
#rm $BINARY
cd ..
