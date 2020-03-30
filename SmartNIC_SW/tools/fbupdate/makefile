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

objects = \
src/fbupdate.o \
src/flash.o \
src/actel.o \
src/actel_common.o \
src/printlog.o \
src/sysmon.o \
src/panic.o \
src/cmd_shared.o \
src/fbcapture_if.o \
src/gecko.o \
src/actel_gecko_flash.o

release: all
debug: all

all: fbupdate

# Name of text file containing build number.
BUILD_NUMBER_FILE=build-number.txt

# Include build number rules.
include buildnumber.mak

.PHONY : clean 
clean:
	rm -f fbupdate $(objects)
ifneq ("$(wildcard src/actel_sf2.o)","")
	rm -f src/actel_sf2.o
endif

.PHONY: rebuild
rebuild: clean all

common_list = \
	buildnumber.mak \
	src/fbupdate.c \
	src/flash.c \
	src/flash.h \
	src/actel.c \
	src/actel.h \
	src/actel_common.c \
	src/actel_common.h \
	src/actel_sf2.h \
	src/actel_gecko_flash.c \
	src/actel_gecko_flash.h \
	src/sysmon.c \
	src/sysmon.h \
	src/printlog.c \
	src/printlog.h \
	src/cmd_shared.c \
	src/panic.c \
	src/panic.h \
	src/gecko.c \
	src/gecko.h \
	src/qspi.c \
	src/qspi.h \
	src/admin_commands.h \
	src/fpgaregs.h \
	src/fbcapture_if.c \
	src/fbcapture_if.h \
	include/FL_FlashLibrary.h \
	src/lib/FL_FlashLibrary.c \
	src/lib/Makefile \
	subrepositories/fbcommon/include/fpgatypes.h \
	subrepositories/fbcommon/include/fpgamodels.h \
	subrepositories/fbcommon/include/fpgadeviceids.h \
	subrepositories/fbcommon/include/serial_list.h

INCLUDES := ./include/FL_FlashLibrary.h

#CFLAGS := -Wall -D_GNU_SOURCE -DUSE_FL_FLASH_LIBRARY -flto -I./include
CFLAGS := -Wall -D_GNU_SOURCE -DUSE_FL_FLASH_LIBRARY $(BUILD_NUMBER_LDFLAGS) -I./include
release: CFLAGS := -DNDEBUG -O3 $(CFLAGS)
debug:   CFLAGS := -DDEBUG -O0 $(CFLAGS)
CC = cc
#LIBS = ./lib/FL_FlashLibrary.o -lrt -lm
LIBS = lib/FL_FlashLibrary.a -lrt -lm

fbupdate : $(BUILD_NUMBER_FILE) FL_FlashLibrary $(objects) src/actel_sf2.o $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) $(objects) src/actel_sf2.o -o fbupdate $(LIBS)

FL_FlashLibrary:
	cd src/lib; $(MAKE); cd -

#fbupdate.o : fpgaregs.h flash.h printlog.h
sysmon.o : fpgaregs.h sysmon.h printlog.h
flash.o : fpgaregs.h flash.h printlog.h
printlog.o : printlog.h
actel.o : fpgaregs.h actel.h
actel_common.o: actel_common.h
panic.o : fpgaregs.h
fbcapture_if.o: fbcapture_if.c fbcapture_if.h

