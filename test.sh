#!/bin/bash
set -e

FILENAME=$(basename $1)
PRODUCED_OUT="${FILENAME%.clj}.out"
PRODUCED_ERR="${FILENAME%.clj}.err"
./compile.sh $1 > $PRODUCED_OUT 2> $PRODUCED_ERR

EXPECTED_OUT="tests/${FILENAME%.clj}.out"

if [ -f $EXPECTED_OUT ]; then
  diff $PRODUCED_OUT $EXPECTED_OUT
fi

FileCheck --allow-unused-prefixes $1 < $PRODUCED_ERR

echo "Success!"
