#!/bin/bash

TIME=

while getopts t opt; do
  case $opt in
    t)
      TIME=true
      ;;
    ?)
      echo "Invalid option: -${OPTARG}."
      exit 1
      ;;
  esac
done

shift $(($OPTIND - 1))
echo $1
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

if $TIME; then
    time $LEIN run $FILENAME
else
    $LEIN run $FILENAME
fi
rm "$FILENAME"
if [ -f "$BINARY" ]; then
    echo "Success!!"
else
    exit 1
fi
mv $BINARY ../backend
cd ../backend
echo "Executing..."
# lldb ./clojure-rt 
./clojure-rt $BINARY
rm $BINARY
cd ..
