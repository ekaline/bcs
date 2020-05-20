#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters"
    echo "Usage: $0 <image.bit>"
    exit 1
fi

#MDFROMNAME=$(echo `basename $FWBIT` | cut -d_ -f1)
#MD5SUM=$(md5sum $FWBIT | cut -d' ' -f1)
#echo "Expected MD5 snippet $MDFROMNAME"
#
#if [[ $MD5SUM =~ .*$MDFROMNAME.* ]]
#then
#   echo "MD5 Matches"
#else
#   echo "Error: MD5 mismatch, actual MD5 is $MD5SUM"
#   exit 1
#fi

sudo killall sn_state

FULL_BIT=$(readlink -f $1)

sudo /opt/ekaline/SmartNIC_SW/driver/reload_driver.sh
/opt/ekaline/SmartNIC_SW/tools/fbupdate --flash $FULL_BIT
sudo /opt/ekaline/SmartNIC_SW/driver/reload_fw.sh
sudo /opt/ekaline/SmartNIC_SW/driver/reload_driver.sh
