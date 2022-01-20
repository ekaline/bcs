#!/bin/bash

if [[ $1 -eq 0 ]]
then
    echo "No gap inteval specified"
    echo gap=\'$1\' delta=\'$2\'
else
    gap=$1
    delta=$2
    echo gap=\'$gap\' delta=\'$delta\'
    make GCC_EXTERNAL_DEFINES="-D_EFH_TEST_GAP_INJECT_INTERVAL__=$gap -D_EFH_TEST_GAP_INJECT_DELTA_=$delta" -j 64
fi
