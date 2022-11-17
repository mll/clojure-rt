#!/bin/bash

LEIN=`which lein`
FILENAME=$1
echo $FILENAME
cp $FILENAME frontend
cd frontend
$LEIN run $FILENAME
mv *.cljb ../backend
rm $FILENAME
cd ../backend
#arch -arm64 lldb ./clojure-rt 
./clojure-rt *.cljb
#rm *.cljb
cd ..