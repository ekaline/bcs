#!/bin/bash

TEST_LIST="
efcFastSweepFireTestNew
efcCmeFireTestNew
efcCboeTestNew
efcNewsFireTest
"



RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

RESULT=()
ME_DIR=$(readlink -f "$(dirname "$0")")

for f in $TEST_LIST
do
    echo "------------------------------------------------------------------------------------------"
    echo "Executiing $f -e"
    echo "------------------------------------------------------------------------------------------"

    $ME_DIR/../build/tests/ekaN/$f -e

    retVal=$?
    if [ $retVal -ne 0 ]; then
	RESULT+=(0)
    else
	RESULT+=(1)
    fi

done

echo ""
echo "------------------------------------------------------------------------------------------"
echo "------------------------------------------------------------------------------------------"
c=0
for f in $TEST_LIST
do
    if [ ${RESULT[c]} == 1 ]; then
	echo -e "TEST $f $GREEN PASS $NC"
    else
	echo -e "TEST $f $RED FAIL $NC"
    fi
    c=$(expr $c + 1)
done
echo "------------------------------------------------------------------------------------------"
echo "------------------------------------------------------------------------------------------"
