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

# This script is example code from Silicom Denmark A/S
# It may not work on all servers
# This script finds the PCI bus and card addresses for one or more Silicom SmartNIC cards
# and reboots and reloads that PCI devices
# The smartnic driver will be unloaded by the script, if not possible the script will stop

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
FIBERBLAZE_ROOT=$SCRIPTPATH/..

FBCONFIG=$FIBERBLAZE_ROOT/tools/fbconfig
FBUPDATE=$FIBERBLAZE_ROOT/bin/fbupdate


# Default card device
DEFAULT_DEV=smartnic

module="smartnic"
dryrun=0
force_rescan=1
DEV=
verbose=0
wait_before_rescan=0
debug=0
list_devices=0
force_noreload=0



error() {
    local parent_lineno="$1"
    local message="$2"
    local code="${3:-1}"
    if [[ -n "$message" ]] ; then
        echo "ERROR: Script failed:"
        echo " - Error on or near line ${parent_lineno}: ${message};"
        echo " - exiting with status ${code}"
        echo "    line: $(caller 0)"
    else
        echo "ERROR: Script failed:"
        echo " - Error on or near line ${parent_lineno};"
        echo " - exiting with status ${code}"
        echo "    line: $(caller 0)"
    fi
    echo
    exit "${code}"
}
trap 'error ${LINENO}' ERR


cond_echo() {
    if [ $verbose -ge $1 ] ; then 
        echo "$2"
    fi;
}

# read arguments
while [[ $# -gt 0 ]]
do
    key="$1"

    case $key in
        -d|--device)
            if [ -z ${DEV} ] ; then
                DEV=$2
                if [[ "$2" == "" ]]
                then
                    echo "<card name> argument missing"
                    exit 1
                fi
            else 
                echo "Error: only one -d is allowed!"
                exit -1
            fi
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [-h][-d <card name>] ... [--rescan][--dry-run] "
            echo "Example script to reboot, and rescan for SmartNIC <card name>s. Default is $module."
            echo "-d | --device  : Specify the device to reload. Default is smartnic"
            echo "--rescan       : to do the rescan. Which should bring back the board with the new image loaded into the FPGA(this is default behavior)"
            echo "--norescan     : to NOT do the rescan. "
            echo "--noreload     : do NOT run the fbupdate tool for reloading the PGA from flash. - useful when you want to program through JTAG"
            echo "--dry-run      : to do a dry run that only prints the PCI bus device files found."
            echo "-v | --verbose : be verbose about what is going on."
            echo "-vv            : be extra verbose about what is going on."
            echo "-w | --wait    : Wait for user input before doing the rescan - useful when programming through JTAG"
            echo "-l | --list    : List SmartNIC device names and associated pci domain/bus/slot/func in the format domain.bus.slot.func"
            echo "--debug        : log additional debugging info."
            echo "If the $module driver is loaded, it will be unloaded, so any application using the driver must be stopped."
            exit 0
            ;;
        --rescan)
            force_rescan=1
            ;;
        --norescan)
            force_rescan=0
            ;;
        --noreload)
            force_noreload=1
            ;;
        --dry-run)
            dryrun=1
            ;;
        -w|--wait)
            wait_before_rescan=1
            ;;
        -v|--verbose)
            verbose=1
            ;;
        -vv)
            verbose=2
            ;;
        -l|--list)
            list_devices=1
            ;;
        --debug)
            debug=1
            verbose=2
            ;;
        *)
            echo "Unknown option: ${key}"
            exit 1
            ;;
    esac
    shift
done

if [ $verbose -ge 2 ] ; then
    minus_v="-v"
else
    minus_v=""
fi

# If no device(s) specified use default
if [ -z $DEV ] ; then
    DEV=$DEFAULT_DEV
fi
echo "Device to be reloaded: $DEV"

if [ $debug -ge 1 ] ; then 
    echo "  - $(date "+%H:%M:%S.%N") :: Log lspci"
    sudo lspci -vvv  > lspci_all_first.txt
fi

# Load driver if it is not loaded and exit if it cannot be loaded
echo "Load driver if not already loaded:"
for i in {0,1}; do
    driver_load=$(lsmod|grep $module|cut -f1 -d' ')
    if [[ ! "${driver_load}" == $module ]]; then
        cond_echo 1 "  Seems that driver was not loaded - loading it."
       
        if [[ $i -eq 0 ]]; then
            echo -ne "  "
            sudo ${FIBERBLAZE_ROOT}/driver/load_driver.sh
        else
            echo "  Cannot load the Silicom $module driver. Is card installed? Please load driver."
            exit 1
        fi
    else
        cond_echo 1 "  Driver is loaded."
        break
    fi
done

if ! [ -e /dev/$DEV ] ; then
    echo "  Seems that driver was not loaded, and that load failed!"
    echo "   - No /dev/$DEV was found!"
    exit -1
fi

if [[ $list_devices -eq 1 ]] ; then
    echo "List SmartNIC devices currently active:"
    for i in $(ls -1 /dev/smartnic[0-9]*); do 
        if ${FBCONFIG} -d /dev/$DEV -P > /dev/null 2>&1 ; then 
            pci_device=$(${FBCONFIG} -d $i -P | sed 's/^PCIe device name: //g')
            if [ $? -ne 0 ] ; then
                echo
                echo "ERROR: Can not continue!!!"
                exit -1
            fi
        else 
            #Use alternative unsafe method
            fbconf_out=$(dmesg -t | grep "Silicom SmartNIC" | grep $DEV | sed 's/^.*: Silicom SmartNIC - PCIe //g' | cut -d"," -f1 | tail -n1)
        fi 
        fbupda_out=$( ${FBUPDATE} -d $i )
        if [ $? -ne 0 ] ; then
            echo
            echo "ERROR: Can not continue!!!"
            exit -1
        fi
        phy_slot=$(sudo lspci -vm -s$pci_device | grep PhySlot | cut  -f2)
        if [ -z $phy_slot ] ; then
            echo -ne "$i: PCIe device name: $pci_device - $fbupda_out"
        else 
            echo -ne "$i: PCIe device name: $pci_device - $fbupda_out - Physical slot: $phy_slot"
        fi
        echo
    done
    exit 0
fi

# Figure out the pci_device name from the /dev/ device name:
echo "Determining PCIe device used by driver: $DEV"
if ${FBCONFIG} -d /dev/$DEV -P > /dev/null 2>&1 ; then 
    cond_echo 1 "  Trying preferred method"
    pci_device=$(${FBCONFIG} -d /dev/$DEV -P | sed 's/^PCIe device name: //g')
else
    cond_echo 1 "  \"fbconfig -d /dev/$DEV -P\" seems to fail - trying to extract info from dmesg."
    pci_device=$(dmesg -t | grep "Silicom SmartNIC" | grep $DEV | sed 's/^.*: Silicom SmartNIC - PCIe //g' | cut -d"," -f1 | tail -n1)
    if [ -z $pci_device ] ; then
        echo "Unable to determine pcie_device of \"$DEV\" - tried both of fbconfig -P and scan dmesg"
        echo "Unable to proceed - try reloading the driver and then rerun this script"
        exit -1
    fi
fi
echo "  '$DEV' uses PCIe device: $pci_device "
if [ -z $pci_device ] ; then
    echo "No $module PCIe devices found in /sys/devices. Is card installed?"
    exit 1
fi

pushd /sys/devices > /dev/null

DEVICE=$(find /sys/devices -name $pci_device)

echo "Reload card at pos: $DEVICE"

REMOVE=$DEVICE/remove
RESCAN=$(dirname $DEVICE)/rescan
RESET_CARD=$DEVICE/reset
ROOT_PORT=$(basename $(dirname $DEVICE))
DEVICE_PORT=$(basename $DEVICE)
if [ $verbose -ge 2 ] ; then 
    echo " - REMOVE      = $REMOVE"
    echo " - RESCAN      = $RESCAN"
    echo " - RESET_CARD  = $RESET_CARD"
    echo " - ROOT_PORT   = $ROOT_PORT"
    echo " - DEVICE_PORT = $DEVICE_PORT"
    echo
    echo "REMOVE=$REMOVE; RESCAN=$RESCAN; RESET_CARD=$RESET_CARD; ROOT_PORT=$ROOT_PORT; DEVICE_PORT=$DEVICE_PORT"
fi

popd > /dev/null

if [[ $dryrun -ne 1 ]] ; then
    # Check that driver can be unloaded and reload it again
    echo "Check that driver can be unloaded and reload it again"
    sudo rmmod $module
    if [[ $? -ne 0 ]] ; then
        echo "The $module driver could not be unloaded. Please load driver or stop processes using the driver"
        exit 1
    fi
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Load driver:"; fi
    echo -ne "  "
    sudo ${FIBERBLAZE_ROOT}/driver/load_driver.sh
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") ::   Driver loaded"; fi
    if [ $debug -ge 1 ] ; then 
        echo "  - $(date "+%H:%M:%S.%N") ::   Log lspci"
        sudo lspci -vvv  > lspci_all_mid1.txt
    fi
    cond_echo 1 "  Done."
    
    echo

    cond_echo 1 "Save selected PCI config space:"
    # Save some registers from the endpoint:
    # - save "Command register"
    saved_command=$(sudo setpci -s $DEVICE_PORT COMMAND)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: COMMAND=$saved_command"; fi;
    # - save "Device Control Register"
    saved_dev_ctrl=$(sudo setpci -s $DEVICE_PORT CAP_EXP+0x08.w)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: DevCtrl=$saved_dev_ctrl"; fi;
    # - save "Cache Line Size Register"
    saved_cls=$(sudo setpci -s $DEVICE_PORT CACHE_LINE_SIZE)

    # Save some AER (Advanced Error Reporting) registers:
    #  - save AER: Uncorrectable Error Mask Register 
    saved_aer_uemsk=$(sudo setpci -s $DEVICE_PORT ECAP_AER+0x08.l)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: AER uemsk=$saved_aer_uemsk"; fi;
    #  - save AER: Uncorrectable Error Severity Registe
    saved_aer_uesvrt=$(sudo setpci -s $DEVICE_PORT ECAP_AER+0x0c.l)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: AER uesvrt=$saved_aer_uesvrt"; fi;
    #  - save AER: Correctable Error Mask Register
    saved_aer_cemsk=$(sudo setpci -s $DEVICE_PORT ECAP_AER+0x14.l)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: AER cemsk=$saved_aer_cemsk"; fi;

    if [ $debug -ge 1 ] ; then 
        echo "  - $(date "+%H:%M:%S.%N") :: Log lspci"
        sudo lspci -vvv  > lspci_all_mid2.txt
    fi
    cond_echo 1 "  Done."
    
    #####################################################################################################################################################
    # Example code to reboot card delayed, remove from PCI bus before reboot and rescan bus afterwards
    # This may not work on all servers!!!
    if [ $force_noreload -eq 0 ] ; then
        echo "Reload FPGA from flash:"
        reprog_delay=4
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: fbupdate -d $DEV -c bf reboot probe ${reprog_delay}000"; fi
        fbupdate_res=$(${FBUPDATE} -d $DEV -c bf reboot probe ${reprog_delay}000)
        if [ $verbose -ge 1 ] ; then echo "  $fbupdate_res" | sed 's/Remember to reboot PC!//g';  fi
        
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") ::   Wait 1 sec for cmd to take effect"; fi
        sleep 1
        cond_echo 1 "  Done."
    else
        echo "Reload FPGA from flash DISABLED - so skipping reload!"
    fi

    echo "Remove driver:"
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Before: sudo rmmod $module"; fi;
    # You might not *NEED* to do this, if you for some reason do find that more convenient, the driver might not be happy about it though:
    sudo rmmod $module
    cond_echo 1 "  Done.clear"

    cond_echo 1 "Disabling system error on expected events:"
    # Save the Root port "Bridge Control Register"
    rp_saved_bridge_control=$(sudo setpci -s $ROOT_PORT BRIDGE_CONTROL)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: saved \"Bridge Control Register\" : $rp_saved_bridge_control"; echo -ne "      "; fi;
    # On Root Port in "Bridge Control Register" disable "Parity Error Response Enable" and "SERR# Enable"
    sudo setpci $minus_v -s $ROOT_PORT BRIDGE_CONTROL=0:0003
    
    # Save the Root Port "Device Control Register"
    rp_saved_device_control=$(sudo setpci -s $ROOT_PORT CAP_EXP+0x08.w)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: saved \"Device Control Register\" : $rp_saved_device_control"; fi;
    # On the Root port in "Device Control Register" disable "Fatal Error Reporting" and "Non-Fatal Error Reporting"
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: In \"Device Control Register\" disable \"Fatal Error Reporting\" and \"Non-Fatal Error Reporting\" "; echo -ne "      "; fi
    sudo setpci $minus_v -s $ROOT_PORT CAP_EXP+0x08.w=0000:0006

    # Save the Root Port "Root Control Register"
    rp_saved_root_ctl=$(sudo setpci -s $ROOT_PORT CAP_EXP+0x1c.w)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: saved \"Root Control Register\" :$rp_saved_root_ctl"; fi;
    # On Root Port in "Root Control Register" disable all "System Error on ** Error Enable" bits
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: In \"Root Control Register\" disable \"System Error on ...\""; echo -ne "      "; fi;
    sudo setpci $minus_v -s $ROOT_PORT CAP_EXP+0x1c.w=0:0007

    # Save Root Port "Command Register"
    saved_rp_command=$(sudo setpci -s $ROOT_PORT COMMAND)
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: In \"Command Register\" disable INTx and SERR#"; echo -ne "      "; fi;
    # On Root Port in "Command Register" disable "INTx" and "SERR#"
    sudo setpci $minus_v -s $ROOT_PORT COMMAND=0x400:0x500
    cond_echo 1 "  Done."

    # Remove the PCIe device (The FPGA)
    echo "Remove pci device:"
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Before: sudo bash -c \"echo 1 >$REMOVE\" "; fi;
    sudo bash -c "echo 1 >$REMOVE"
    cond_echo 1 "  Removed $(dirname $REMOVE)"

    #Bring link down:
    cond_echo 1  "  Bring link down"
    if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: In \"Link Control Register\" set \"Link Disable\""; echo -ne "      "; fi;
    sudo setpci $minus_v -s $ROOT_PORT CAP_EXP+10.w=0010:0010
    sleep 1

    if [ $debug -ge 1 ] ; then 
        echo "  - $(date "+%H:%M:%S.%N") :: Log lspci"
        sudo lspci -vvv  > lspci_all_mid3.txt
    fi
    cond_echo 1 "  Done."

    ####### Wait for user input ###################################################################################################

    if [[ $wait_before_rescan -eq 1 ]] ; then
        echo 
        echo "You can now program the FPGA via JTAG"
        read -p "Type <Enter> to continue"
    fi
 
    ####### Rescan section ###################################################################################################
    if [[ $force_rescan -eq 1 ]] ; then
        wait_time=$(($reprog_delay + 3))
        #echo "Wait $wait_time sec for FPGA to get reloaded"
        echo "Wait for FPGA to get reloaded:"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Wait $wait_time sec for FPGA to get reloaded:"; fi; 
        for i in $(seq $wait_time -1 1 ) ; do 
            if [ $verbose -ge 2 ] ; then 
                echo -ne "\r    - $(date "+%H:%M:%S.%N") :: $i  "; 
            else 
                echo -ne "."
            fi; 
            sleep 1; 
        done 
        echo

        echo "Rescanning for removed devices:"
        #Bring link up:
        cond_echo 1  "  Bring link up"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: In \"Link Control Register\" clear \"Link Disable\""; echo -ne "      "; fi;
        sudo setpci $minus_v -s $ROOT_PORT CAP_EXP+10.w=0000:0010
        sleep 1

        # Start Equalization - seems that ECAP19 is not available on all switches
        cond_echo 1  "  Is Equalizationi supported?"
        if sudo setpci -s $ROOT_PORT ECAP19.l > /dev/null 2>&1; then 
            cond_echo 1  "  Equalization is supported on this bus"
            if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: In \"Link Control 3 Register\" set \"Perform Equalization\""; echo -ne "      "; fi;
            sudo setpci $minus_v -s $ROOT_PORT ECAP19+04.l=00000001:00000001
            sleep 1
        else 
            cond_echo 1  "  - NOTE: Equalization is NOT supported on this bus ($ROOT_PORT) - skipped"
            cond_echo 1  "  - - You might not be on a gen3 bus"
        fi

        # Retrain Link
        cond_echo 1 "  Retrain Link"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: In \"Link Control Register\" set \"Retrain Link\""; echo -ne "      "; fi;
        sudo setpci $minus_v -s $ROOT_PORT CAP_EXP+10.w=0020:0020
        sleep 1

        # Enforce a rescan for the device in the FPGA
        sleep 1; #This sleep is essential
        cond_echo 1 "  Rescan"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Before: sudo bash -c \"echo 1 >$RESCAN\" "; fi;
        sudo bash -c "echo 1 > $RESCAN"
        sleep 1
        cond_echo 1 "  Rescaned $(dirname $RESCAN)"
        cond_echo 1 "  Done."

        # Ensure a proper Reset of FPGA logic
        cond_echo 1 "Resetting devices:"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Before: sudo bash -c \"echo 1 >$RESET_CARD\" "; fi;
        sudo bash -c "echo 1 >$RESET_CARD"
        cond_echo 1 "  Reset of $RESET_CARD done."

        cond_echo 1 "Re-enabling system error on unexpected events:"
        # Restore "Command Register"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Restore \"Command Register\" = $saved_command"; echo -ne "      ";fi;
        sudo setpci $minus_v -s $DEVICE_PORT COMMAND=$saved_command

        # Restore "Device Control Register"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Restore \"Device Control Register\" = $saved_dev_ctrl"; echo -ne "      "; fi;
        sudo setpci $minus_v -s $DEVICE_PORT CAP_EXP+0x08.w=$saved_dev_ctrl

        # Restore "Cache Line Size Register"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Restore \"Cache Control Register\" = $saved_cls"; echo -ne "      "; fi;
        sudo setpci $minus_v -s $DEVICE_PORT CACHE_LINE_SIZE=$saved_cls

        # On Root Port in "Bridge Control Register" reenable "Parity Error Response Enable" "and SERR# Enable"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: Re-enable: \"Bridge Control\": \"Parity Error Response Enable\" and \"SERR# Enable\" "; echo -ne "      "; fi;
        sudo setpci $minus_v -s $ROOT_PORT BRIDGE_CONTROL=${rp_saved_bridge_control}:0003
        
        # On Root Port in "Command Register" reenable "INTx" and "SERR#"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: Re-enable: \"Command Register\": \"SERR\" and \"INTx\""; echo -ne "      "; fi;
        sudo setpci $minus_v -s $ROOT_PORT COMMAND=$saved_rp_command:0x500

        # On Root Port in "Device Control Register" reenable "Fatal Error Reporting" and "Non-Fatal Error Reporting"
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: Re-enable: \"Device Control Register\": \"Fatal Error Reporting\" and \"Non-Fatal Error Reporting\""; echo -ne "      "; fi;
        sudo setpci $minus_v -s $ROOT_PORT CAP_EXP+0x08.w=$rp_saved_device_control:0006

        # On Root Port in "Root Control Register" re-enable "System Error on ** Error Enable" bits
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: Re-enable \"Root Control Register\": \"System Error on ** Error Enable\" -bits "; echo -ne "      "; fi
        sudo setpci $minus_v -s $ROOT_PORT CAP_EXP+0x1c.w=${rp_saved_root_ctl}:0007
        # This could have Killed the server so lets write some progress info
        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Root Port: \"Root Control register\" restored"; fi;

        if [ $debug -ge 1 ] ; then 
            echo "  - $(date "+%H:%M:%S.%N") :: Log lspci"
            sudo lspci -vvv  > lspci_all_mid4.txt
        fi

        if [ $verbose -ge 2 ] ; then echo "  - $(date "+%H:%M:%S.%N") :: Clear some status bits that might have been set during reload:"; fi
        #Clear some status bits:
        # - AER (Advanced Error Reporting): 
        # - - AER: Uncorrectable Error Status Register 
        if [ $verbose -ge 2 ] ; then echo -ne "      "; fi
        sudo setpci $minus_v -s $DEVICE_PORT ECAP_AER+04.l=ffffffff
        # - - AER: Uncorrectable Error Mask Register 
        if [ $verbose -ge 2 ] ; then echo -ne "      "; fi
        sudo setpci $minus_v -s $DEVICE_PORT ECAP_AER+0x08.l=$saved_aer_uemsk
        # - - AER: Uncorrectable Error Severity Registe
        if [ $verbose -ge 2 ] ; then echo -ne "      "; fi
        sudo setpci $minus_v -s $DEVICE_PORT ECAP_AER+0x0c.l=$saved_aer_uesvrt
        # - - AER: Correctable Error Status Register
        if [ $verbose -ge 2 ] ; then echo -ne "      "; fi
        sudo setpci $minus_v -s $DEVICE_PORT ECAP_AER+10.l=ffffffff
        # - - AER: Correctable Error Mask Register
        if [ $verbose -ge 2 ] ; then echo -ne "      "; fi
        sudo setpci $minus_v -s $DEVICE_PORT ECAP_AER+0x14.l=$saved_aer_cemsk

        # - Device Status Register
        if [ $verbose -ge 2 ] ; then echo -ne "      "; fi
        sudo setpci $minus_v -s $ROOT_PORT CAP_EXP+0x0a.w=f

        if [ $debug -ge 1 ] ; then 
            echo "  - $(date "+%H:%M:%S.%N") :: Log lspci"
            sudo lspci -vvv  > lspci_all_last.txt
        fi
        cond_echo  1 "  Done."

        echo
        echo "Board should now be ready for use. You can load the driver now."
        echo
    fi
fi
