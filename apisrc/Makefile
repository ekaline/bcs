SMARTNIC_ROOT:=../SmartNIC_SW
LWIP_ROOT:=../lwip
ARCH_FLAGS := -march=native
CC_OPT:= -O3 -g

#ARCH_FLAGS := -march=corei7-avx -mmmx -msse -mpreferred-stack-boundary=4

CC := g++ -std=c++17 $(CC_OPT) -frecord-gcc-switches -pipe $(ARCH_FLAGS)

LIBEKA_GIT_VER:= $(shell git rev-parse --short HEAD)

DRIVERS_INCL:=$(SMARTNIC_ROOT)/driver/
API_INCL:=$(SMARTNIC_ROOT)/include/

LWIP_INCL:=$(LWIP_ROOT)/lwip/src/include 
INC=$(DRIVERS_INCL) $(API_INCL) $(LWIP_INCL) $(shell pwd) $(shell pwd)/compat $(shell pwd)/FhBook $(shell pwd)/../pcapParsers
INCL=$(foreach d, $(INC), -I$d)
INCL+=-Iarch

include $(SMARTNIC_ROOT)/api/makefile_common

#GCC_FLAGS:=-Wall -fpic -pthread -lrt $(EKA_AFFINITY_SET) -lm $(GCC_EXTERNAL_DEFINES) -DBUILD_TIME=`date +%d%m%y_%T`
GCC_FLAGS:=-Wall -fPIC -pthread -lrt -lm $(GCC_EXTERNAL_DEFINES) -Wno-unused-function -DLIBEKA_GIT_VER=\"$(LIBEKA_GIT_VER)\"
#LD_FLAGS:=-shared -fpic $(LD_ASAN_OPTIONS) -static-libstdc++ -static-libgcc
LD_FLAGS:=-shared -fpic $(LD_ASAN_OPTIONS)


#EKAUTILS_SRCS := $(wildcard ../eka_utils/*.c)
#EKAUTILS_EXECS := $(patsubst %.c,%,$(EKAUTILS_SRCS))

EKAUTILS := $(patsubst %.c,%,$(wildcard ../eka_utils/*.c))

EKATESTS_SRCS := $(wildcard ../new_api_tests/e*.c ../new_api_tests/tcp*.c)
EKATESTS_EXECS := $(patsubst %.c,%,$(EKATESTS_SRCS))

UNITTESTS_SRCS := $(wildcard ../new_api_tests/unit*.c)
UNITTESTS_EXECS := $(patsubst %.c,%,$(UNITTESTS_SRCS))

EKALIB_SRCS := $(wildcard e*.c E*.c)
EKALIB_SRCS += $(wildcard FhBook/*.c)

EKALIB_FILES := $(patsubst %.c,%,$(EKALIB_SRCS))
EKALIB_OBJ := $(EKALIB_SRCS:.c=.o)
LLIB := -L. -lekaline2
LLIB_TEST := -L. -lekaline2_test
SMARTNIC_LIB := $(SMARTNIC_ROOT)/lib/libsmartnic.a

LWIP_LIB := liblwipcommon.a

CCDEP:=gcc

LIBEKALINE:=libekaline2

SMARTNIC_LIB_FILES := $(shell ar t $(SMARTNIC_LIB))

#all:  $(LIBEKALINE) $(EKAUTILS_EXECS) $(EKATESTS_EXECS) 
all:  eka_utils ekalib ekalib_test eka_tests unit_tests $(LIBEKALINE).a libeka_depend lwip_depend
	@echo "========================================="
	@echo created: $^

lab: GCC_EXTERNAL_DEFINES=-DEKA_TEST_IGNORE_DEFINITIONS -DEKA_TEST_IGNORE_GAP -DFH_LAB
lab: GCC_FLAGS= -Wall -fpic -pthread -lrt -lm $(GCC_EXTERNAL_DEFINES)
lab: all

use_onload: export LD_PRELOAD=libonload.so
use_onload: all

###############################################
# LWIP

LWIPDIR?=$(LWIP_ROOT)/lwip/src
CFLAGS+=-fPIC -I. -Og -I$(LWIPDIR)/include -I./arch -pthread -lrt -D_GNU_SOURCE 

LDFLAGS+=-lutil -pthread -lrt -fPIC

include $(LWIPDIR)/Filelists.mk

LWIPFILES=$(LWIPNOAPPSFILES)  sys_arch.c

LWIPOBJS=$(notdir $(LWIPFILES:.c=.o))

LWIPLIBCOMMON=liblwipcommon.a
#$(LWIPLIBCOMMON): $(LWIPOBJS)
$(LWIPLIBCOMMON): $(LWIPFILES) $(LWIPOBJS)
	$(AR) $(ARFLAGS) $(LWIPLIBCOMMON) $?
#	rm -f $(LWIPOBJS)

#$(LWIPOBJS): %.o: $(LWIPFILES)
$(LWIPOBJS): %.o:
	$(CCDEP) $(CFLAGS) -c $<

lwip_depend: .lwip_depend
include .lwip_depend

.lwip_depend: $(LWIPFILES)
	$(CCDEP) $(CFLAGS) -I . -MM $^ > .lwip_depend || rm -f .lwip_depend

###############################################
$(EKALIB_OBJ): %.o:
	$(CC) $(CC_OPT) -c $< $(GCC_FLAGS) $(INCL) -o $@


libeka_depend: .libeka_depend
include .libeka_depend

.libeka_depend: $(EKALIB_SRCS)
	$(CC) $(INCL) -MM $^ > .libeka_depend || rm -f .libeka_depend

ekalib: $(LIBEKALINE).so 
$(LIBEKALINE).so: $(EKALIB_OBJ) $(SMARTNIC_LIB) $(LWIP_LIB)
	$(CC) $(EKALIB_OBJ) -L. $(SMARTNIC_LIB) $(LWIP_LIB) -o $@ $(LD_FLAGS) -Wl,--version-script=./compat/$(LIBEKALINE).map -static-libstdc++ -static-libgcc

ekalib_test: $(LIBEKALINE)_test.so 
$(LIBEKALINE)_test.so: $(EKALIB_OBJ) $(SMARTNIC_LIB) $(LWIP_LIB)
	$(CC) $(EKALIB_OBJ) -L. $(SMARTNIC_LIB) $(LWIP_LIB) -o $(LIBEKALINE)_test.so $(LD_FLAGS)


$(LIBEKALINE).a: $(EKALIB_OBJ) $(SMARTNIC_LIB) $(LWIPOBJS)
	ar x $(SMARTNIC_LIB)
	ar rcs $@ $(EKALIB_OBJ) $(SMARTNIC_LIB_FILES) $(LWIPOBJS)

eka_utils: $(EKAUTILS)
$(EKAUTILS): %: %.c $(LIBEKALINE).a
	$(CC) -o $@ $^ $(GCC_FLAGS) $(INCL) -static-libstdc++ -static-libgcc

eka_tests: $(EKATESTS_EXECS)
../new_api_tests/%: ../new_api_tests/%.c $(LIBEKALINE)_test.so
	$(CC) $< -o $@ $(GCC_FLAGS) $(INCL) $(LLIB_TEST)

unit_tests: $(UNITTESTS_EXECS)
../new_api_tests/unit%: ../new_api_tests/unit%.c $(LIBEKALINE)_test.so
	$(CC) $< -o $@ $(GCC_FLAGS) $(INCL) $(LLIB_TEST) $(SMARTNIC_LIB)


../pcapParsers/cmePcapParse.o: ../pcapParsers/cmePcapParse.c 
	$(CC) $(CC_OPT) -c $< $(GCC_FLAGS) $(INCL) -o $@

../pcapParsers/cmePcapParse: ../pcapParsers/cmePcapParse.o EkaFhCmeParser.o eka_common.o
	$(CC) -o $@ $^ $(GCC_FLAGS) $(INCL)

clean:
	rm -f /tmp/core* *.o */*.o *.so *.a *~ core $(EKATESTS_EXECS) $(UNITTESTS_EXECS) $(EKAUTILS_EXECS) */*~ ../*/*~ *.csv *.txt
#	rm -f /tmp/core* *.o */*.o *.so *.a $(LWIPOBJS) *~ .depend core $(EKATESTS_EXECS) $(UNITTESTS_EXECS) $(EKAUTILS_EXECS) */*~ ../*/*~ *.csv *.txt


