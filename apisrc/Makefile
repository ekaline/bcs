ifeq ($(shell whoami),sysavtekaline)
  SMARTNIC_ROOT:=$(HOME)/SmartNIC_SW
  LWIP_ROOT:= $(HOME)/lwip
  ARCH_FLAGS := -march=native
else
  SMARTNIC_ROOT:=$(HOME)/ekatools/smartnic/SmartNIC_SW_3_0_2_RC_WC6
  LWIP_ROOT:=/home/vitaly/ekatools/smartnic/lwip/
  ARCH_FLAGS := -march=native
endif

SMARTNIC_ROOT:=../SmartNIC_SW
LWIP_ROOT:=../lwip
ARCH_FLAGS := -march=native
CC_OPT:= -O3 -g

#ARCH_FLAGS := -march=corei7-avx -mmmx -msse -mpreferred-stack-boundary=4

CC := g++ -std=c++17 $(CC_OPT) -frecord-gcc-switches -pipe $(ARCH_FLAGS)


DRIVERS_INCL:=$(SMARTNIC_ROOT)/driver/
API_INCL:=$(SMARTNIC_ROOT)/include/
#PICO_TCP_INCL:=$(PICOTCP_ROOT)/include $(PICOTCP_ROOT)/build/include
#LWIP_INCL:=$(LWIP_ROOT)/lwip/src/include $(LWIP_ROOT)/contrib-2.1.0/ports/unix/port/include
LWIP_INCL:=$(LWIP_ROOT)/lwip/src/include 
INC=$(DRIVERS_INCL) $(API_INCL) $(LWIP_INCL) $(shell pwd) $(shell pwd)/compat
INCL=$(foreach d, $(INC), -I$d)
INCL+=-Iarch

include $(SMARTNIC_ROOT)/api/makefile_common

#GCC_FLAGS:=-Wall -fpic -pthread -lrt $(EKA_AFFINITY_SET) -lm $(GCC_EXTERNAL_DEFINES) -DBUILD_TIME=`date +%d%m%y_%T`
GCC_FLAGS:=-Wall -fPIC -pthread -lrt -lm $(GCC_EXTERNAL_DEFINES)
LD_FLAGS:=-shared -fpic $(LD_ASAN_OPTIONS) -static-libstdc++ -static-libgcc


EKAUTILS_SRCS := $(wildcard ../eka_utils/*.c)
EKAUTILS_EXECS := $(patsubst %.c,%,$(EKAUTILS_SRCS))

EKATESTS_SRCS := $(wildcard ../new_api_tests/e*.c)
EKATESTS_EXECS := $(patsubst %.c,%,$(EKATESTS_SRCS))

UNITTESTS_SRCS := $(wildcard ../new_api_tests/unit*.c)
UNITTESTS_EXECS := $(patsubst %.c,%,$(UNITTESTS_SRCS))

EKALIB_SRCS := $(wildcard *.c)

EKALIB_FILES := $(patsubst %.c,%,$(EKALIB_SRCS))
EKALIB_OBJECTS := $(EKALIB_SRCS:.c=.o)
LLIB := -L. -lekaline2
LLIB_TEST := -L. -lekaline2_test
SMARTNIC_LIB := $(SMARTNIC_ROOT)/lib/libsmartnic.a
PICOTCP_LIB := $(PICOTCP_ROOT)/build/lib/libpicotcp.a

LWIP_LIB := liblwipcommon.a

CCDEP:=gcc

LIBEKALLINE:=libekaline2

#all:  $(LIBEKALLINE) $(EKAUTILS_EXECS) $(EKATESTS_EXECS) 
all:  eka_utils ekalib eka_tests unit_tests
	@echo "========================================="
	@echo created: $^

lab: GCC_EXTERNAL_DEFINES=-DEKA_TEST_IGNORE_DEFINITIONS -DEKA_TEST_IGNORE_GAP -DFH_LAB
lab: GCC_FLAGS= -Wall -fpic -pthread -lrt -lm $(GCC_EXTERNAL_DEFINES)
lab: all

###############################################
# LWIP

LWIPDIR?=$(LWIP_ROOT)/lwip/src
CFLAGS+=-fPIC -I. -I$(LWIPDIR)/include

LDFLAGS+=-lutil -pthread -lrt -fPIC

include $(LWIPDIR)/Filelists.mk

LWIPFILES=$(LWIPNOAPPSFILES)

LWIPOBJS=$(notdir $(LWIPFILES:.c=.o))

LWIPLIBCOMMON=liblwipcommon.a
#$(LWIPLIBCOMMON): $(LWIPOBJS)
$(LWIPLIBCOMMON): $(LWIPFILES) $(LWIPOBJS)
	$(AR) $(ARFLAGS) $(LWIPLIBCOMMON) $?
#	rm -f $(LWIPOBJS)

#$(LWIPOBJS): %.o: $(LWIPFILES)
$(LWIPOBJS): %.o:
	$(CCDEP) $(CFLAGS) -c $<

depend dep: .depend
include .depend

.depend: $(LWIPFILES)
	$(CCDEP) $(CFLAGS) -I . -MM $^ > .depend || rm -f .depend

###############################################

eka_%.o: eka_%.c
	$(CC) -c $< $(INCL) $(GCC_FLAGS)

E%.o: E%.c
	$(CC) -c $< $(INCL) $(GCC_FLAGS)

ekalib: $(LIBEKALLINE).so 
$(LIBEKALLINE).so: $(EKALIB_OBJECTS) $(SMARTNIC_LIB) $(LWIP_LIB)
	$(CC) $(EKALIB_OBJECTS) -L. $(SMARTNIC_LIB) $(LWIP_LIB) -o $@ $(LD_FLAGS) -Wl,--version-script=./compat/$(LIBEKALLINE).map
	$(CC) $(EKALIB_OBJECTS) -L. $(SMARTNIC_LIB) $(LWIP_LIB) -o $(LIBEKALLINE)_test.so $(LD_FLAGS)

eka_utils: $(EKAUTILS_EXECS)
../eka_utils/%: ../eka_utils/%.c
	$(CC) $< -o $@ $(GCC_FLAGS) $(INCL) $(SMARTNIC_LIB)
#	$(CC) $< -o $@ $(GCC_FLAGS) -I . -I $(DRIVERS_INCL) -I $(API_INCL) -I ../apisrc $(SMARTNIC_LIB)

eka_tests: $(EKATESTS_EXECS)
../new_api_tests/e%: ../new_api_tests/e%.c $(LIBEKALLINE).so
	$(CC) $< -o $@ $(GCC_FLAGS) $(INCL) $(LLIB)

unit_tests: $(UNITTESTS_EXECS)
../new_api_tests/unit%: ../new_api_tests/unit%.c $(LIBEKALLINE).so
	$(CC) $< -o $@ $(GCC_FLAGS) $(INCL) $(LLIB_TEST) $(SMARTNIC_LIB)


clean:
	rm -f *.o *.so *.a $(LWIPOBJS) *~ .depend core $(EKATESTS_EXECS) $(UNITTESTS_EXECS) $(EKAUTILS_EXECS) */*~ ../*/*~ *.csv *.txt


