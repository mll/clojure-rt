#!/bin/bash

LEIN=`which lein`
TOCOPY=$1
FILENAME=$(basename $TOCOPY)
BINARY="${FILENAME%.clj}.cljb"
cd backend
if [ -f "$BINARY" ]; then
    rm $BINARY
fi
cd ..
if [ -f "$TOCOPY" ]; then
    echo "Compiling to AST..." 
else
    echo "No such file: $TOCOPY"    
    exit 1
fi

cp $TOCOPY frontend
cd frontend

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
