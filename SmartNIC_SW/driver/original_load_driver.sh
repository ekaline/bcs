#!/bin/bash
# ***************************************************************************
# *
# * Copyright (c) 2008-2019, Silicom Denmark A/S
# * All rights reserved.
# *
# * Redistribution and use in source and binary forms, with or without
# * modification, are permitted provided that the following conditions are met:
# *
# * 1. Redistributions of source code must retain the above copyright notice,
# * this list of conditions and the following disclaimer.
# *
# * 2. Redistributions in binary form must reproduce the above copyright
# * notice, this list of conditions and the following disclaimer in the
# * documentation and/or other materials provided with the distribution.
# *
# * 3. Neither the name of the Silicom nor the names of its
# * contributors may be used to endorse or promote products derived from
# * this software without specific prior written permission.
# *
# * 4. This software may only be redistributed and used in connection with a
# *  Silicom network adapter product.
# *
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# * POSSIBILITY OF SUCH DAMAGE.
# *
# ****************************************************************************

module="smartnic"
device="smartnic"
support_ul_devices=0

DIR=$(dirname "$0")
CMDLINE=$*
while [[ $# -gt 0 ]]; do
    case $1 in
        support_ul_devices=1)
            support_ul_devices=1
           ;;
    esac
    shift
done

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# Leonberg device ids
device_ids="1c2c:(0C)|(00(01|C0|C1|C2|C3|C4|C5|C6|C7|C8|C9|CA))"

# Match all SmartNIC, Chemnitz and Leonberg cards (0001 is POC 1 or POC 2):
NOCARDS=$(lspci -nd 1c2c: | egrep -i "$device_ids" | wc -l)

if [ $NOCARDS -eq 0 ]; then
    echo ""
    echo "Can't find any Silicom SmartNIC cards"
    echo ""
    exit 1
fi

# Build device_mapping paramater if it is not already included in CMDLINE:
device_mapping=$(echo "$CMDLINE"|egrep -o "device_mapping=")
if [[ ${#device_mapping} == 0 ]]; then
    device_mapping=$(lspci -Dnd 1c2c: | egrep -i "$device_ids" | cut -d" " -f1 | sort | paste -d "," -s)
    CMDLINE=$(echo "device_mapping=$device_mapping $CMDLINE")
fi

# invoke insmod with all arguments we got
# and use a pathname, as newer modutils don't look in . by default
insmod "${DIR}/$module.ko" "$CMDLINE" || exit 1

# remove stale nodes
rm -f /dev/$device
for minor in $(seq 0 15); do 
    rm -f "/dev/${device}$minor"
done
for minor in $(seq 0 127); do 
    rm -f "/dev/ul$minor"
done

major=$(cat /proc/devices | grep $module | awk '{print $1}')

# Find minor numbers corresponding to the device_mapping
MINORS=""
for minor in $(seq 0 15); do
    MINOR=$(echo "$CMDLINE"|cut -d"," -f$((minor+1)))
    CMDLINE=${CMDLINE#$MINOR}
    if [ ${#MINOR} -gt 0 ]; then MINORS=$MINORS$minor" "; fi
done

# Create SmartNIC devices:
for minor in $MINORS; do
    mknod "/dev/${device}$minor" c "$major" "$minor"
    chmod go+rw "/dev/${device}$minor"
done
# For backward compatibility create symbolic link for /dev/smartnic:
ln -sf /dev/${device}0 /dev/$device

# Create user logic devices /dev/ul0 ... /dev/ul127 (8 per card):
if [ $support_ul_devices -ne 0 ]; then
    # Instead of 8 ports per card factor here should know the sum total of ports in all cards
    for minor in $MINORS; do
        for uldev in $(seq 0 7); do
            mknod /dev/ul$((8*$minor+uldev)) c $major $((16+8*$minor+$uldev))
            sudo chmod go+rw /dev/ul$((8*$minor+$uldev))
        done
    done
fi

echo "Driver '$module' loaded successfully."
