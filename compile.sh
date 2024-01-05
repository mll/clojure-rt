#!/bin/bash

LEIN=`which lein`
FILENAME=$1
BINARY="${FILENAME%.clj}.cljb"
cd backend
if [ -f "$BINARY" ]; then
    rm $BINARY
fi
cd ..
cp $FILENAME frontend
cd frontend
echo "Compiling to AST..." 
time $LEIN run $FILENAME
rm "$FILENAME"
if [ -f "$BINARY" ]; then
    echo "Success!!"
else
    exit 1
fi
mv $BINARY ../backend
cd ../backend
#arch -arm64 lldb ./clojure-rt 
echo "Executing..."
./clojure-rt $BINARY
#rm $BINARY
cd ..
