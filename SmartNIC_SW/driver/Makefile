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

#
# Makefile for Silicom SmartNIC or PacketMover driver
#

PRODUCT := SmartNIC
#PRODUCT := $(shell echo $(shell basename $(shell dirname $(shell pwd)))|cut -d _ -f 1)
LOWER_PRODUCT := $(shell echo $(PRODUCT) | tr A-Z a-z)
UPPER_PRODUCT := $(shell echo $(PRODUCT) | tr a-z A-Z)

ccflags-y += -DSC_$(UPPER_PRODUCT)_API
ccflags-y += -DSC_CTLS_H_INCLUDED_FROM__DRIVER
ccflags-y += -DPRODUCT_COMMON_H="\"$(LOWER_PRODUCT)_common.h\""
ccflags-y += -DPRODUCT="\"$(PRODUCT)\""
ccflags-y += -DLOWER_PRODUCT="\"$(LOWER_PRODUCT)\""

ifdef DEBUG
    ccflags-y += -DDEBUG
endif
ifdef DUMP
    ccflags-y += -DDUMP
endif
ifndef SUPPORT_LOGGING
    ccflags-y += -DSUPPORT_LOGGING=1
endif
ifdef MAX_ARP_ENTRIES
    ccflags-y += -DMAX_ARP_ENTRIES=$(MAX_ARP_ENTRIES)
endif

obj-m += smartnic.o

smartnic-objs += main.o fpga.o connect.o channel.o net.o recv.o oob_chan.o igmp.o util.o panic.o stat.o
smartnic-objs += ndev.o intr.o custom.o flowctrl.o acl.o arp.o mmu.o

KVERSION := $(shell uname -r)
PWD := $(shell pwd)

release: all
debug: all

all: links
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

links:
	@echo "Defined macros = $(ccflags-y)"
	@for i in fpgatypes.h fpgamodels.h fpgadeviceids.h; do if [ ! -f $$i ]; then ln -sf ../subrepositories/FB2022_test/subrepositories/fbcommon/include/$$i $$i; fi; done
	@if [ ! -f smartnic_common.h ]; then ln -sf ../include/smartnic_common.h smartnic_common.h; fi
	@unset GREP_OPTIONS # This option causes driver build to fail on Ubuntu 12.04 and some other older systems!
	@if [ ! -f ../include/release_version.h ]; then ln -sf /net/bohr/var/fiberblaze/releases/Leonberg/night_build/release_version.h ../include/release_version.h; fi
