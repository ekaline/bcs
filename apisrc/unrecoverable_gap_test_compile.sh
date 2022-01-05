#!/bin/bash

if [[ $# -ne 1 ]] || [[ $1 -eq 0 ]]
then
    echo "No gap specified"
else
    gap=$1
    echo gap=\'$gap\'
    make GCC_EXTERNAL_DEFINES="-D_EFH_UNRECOVERABLE_TEST_GAP_INJECT_INTERVAL_=$gap" -j 64
fi
