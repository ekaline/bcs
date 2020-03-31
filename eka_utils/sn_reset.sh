#!/bin/bash

if [ -e  $HOME/ekatools/smartnic/SmartNIC_SW/tools/fbconfig ] ; then
    $HOME/ekatools/smartnic/SmartNIC_SW/tools/fbconfig --resetfpga
elif [ -e /local/1/sysavtekaline/SmartNIC_SW/tools/fbconfig ] ; then
    /local/1/sysavtekaline/SmartNIC_SW/tools/fbconfig --resetfpga
else
    echo fbconfig is not found!!!
fi
