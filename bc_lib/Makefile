ifeq ($(shell hostname),xn02)
	SMARTNIC_ROOT ?= /home/vitaly/sc_driver/SmartNIC_SW
else
	SMARTNIC_ROOT ?= ../SmartNIC_SW
endif

LWIP_ROOT ?= ../LWIP

CCDEP:=gcc
#CCDEP:=$GCC

CC_OPT ?= -O3 -funroll-loops
CXX_STANDARD ?= c++23
ARCH_FLAGS ?= -march=native
DEBUG_FLAGS ?= -g


GCC_FLAGS ?= $(CC_OPT) $(DEBUG_FLAGS) -frecord-gcc-switches -Wall -pipe $(ARCH_FLAGS)
CXXFLAGS ?= -std=$(CXX_STANDARD)
LD_FLAGS ?= -static-libstdc++ -static-libgcc $(LD_ASAN_OPTIONS)
RUNTIME_LD_FLAGS ?= -static-libstdc++ -static-libgcc
LD_LIBS ?= -pthread -lrt -lm

LIBEKA_GIT_VER:= $(shell git rev-parse --short HEAD)

SMARTNIC_DRIVER_INCL ?= $(SMARTNIC_ROOT)/driver
SMARTNIC_API_INCL ?= $(SMARTNIC_ROOT)/include
SMARTNIC_LIB ?= $(SMARTNIC_ROOT)/lib/libsmartnic.a

EKALIB_SRCS_DIR := ../apisrc
BC_SRC_DIR := ../bc_src
BC_INC_DIR := ../bc_include

LWIP_INCL:=$(LWIP_ROOT)/src/include
INC=$(SMARTNIC_DRIVER_INCL) $(SMARTNIC_API_INCL) $(LWIP_INCL) $(shell pwd) $(EKALIB_SRCS_DIR) $(EKALIB_SRCS_DIR)/compat $(EKALIB_SRCS_DIR)/FhBook $(EKALIB_SRCS_DIR)/Ehp $(shell pwd)/../pcapParsers $(shell pwd)/../new_api_tests

INCL=$(foreach d, $(INC), -I$d)
INCL+=-I$(EKALIB_SRCS_DIR) -I$(EKALIB_SRCS_DIR)/arch -I$(BC_INC_DIR)

GCC_SUPPRESSED_WARNINGS:=-Wno-unused-function -Wno-packed-not-aligned

EKALINE_FLAGS := $(GCC_EXTERNAL_DEFINES) -DLIBEKA_GIT_VER=\"$(LIBEKA_GIT_VER)\" $(GCC_SUPPRESSED_WARNINGS)


#EKAUTILS_SRCS := $(wildcard ../eka_utils/*.c)
#EKAUTILS_EXECS := $(patsubst %.c,%,$(EKAUTILS_SRCS))

EKAUTILS := $(patsubst %.c,%,$(wildcard ../eka_utils/*.c))

EKATESTS_SRCS := $(wildcard ../bc_tests/*.c)
EKATESTS_EXECS := $(patsubst %.c,%,$(EKATESTS_SRCS))

UNITTESTS_SRCS := $(wildcard ../new_api_tests/unit*.c)
UNITTESTS_EXECS := $(patsubst %.c,%,$(UNITTESTS_SRCS))

PCAP_PARSERS := $(patsubst %.c,%,$($(wildcard ../pcapParsers/unit*.c)))

EKALIB_SRCS := $(wildcard $(EKALIB_SRCS_DIR)/e*.c $(EKALIB_SRCS_DIR)/E*.c)
EKALIB_SRCS += $(wildcard $(EKALIB_SRCS_DIR)/FhBook/*.c)
EKALIB_SRCS += $(wildcard $(EKALIB_SRCS_DIR)/Ehp/*.c)

EKALIB_SRCS += $(wildcard $(BC_SRC_DIR)/*.c)

EKALIB_FILES := $(patsubst %.c,%,$(EKALIB_SRCS))
EKALIB_OBJ := $(EKALIB_SRCS:.c=.o)
LLIB := -L. -lekaline2
LLIB_TEST := -L. -lekaline2_test

LWIP_LIB := liblwipcommon.a

LIBEKALINE:=libekaline2

SMARTNIC_LIB_FILES := $(shell ar t $(SMARTNIC_LIB))



#all:  $(LIBEKALINE) $(EKAUTILS_EXECS) $(EKATESTS_EXECS)
all:  eka_utils ekalib $(LIBEKALINE).a libeka_depend lwip_depend eka_tests # ekalib_test  unit_tests
	@echo "========================================="
	@echo created: $^

lab: GCC_EXTERNAL_DEFINES=-DEKA_TEST_IGNORE_DEFINITIONS -DEKA_TEST_IGNORE_GAP -DFH_LAB
lab: all

use_onload: export LD_PRELOAD=libonload.so
use_onload: all

###############################################
# LWIP

LWIPDIR?=$(LWIP_ROOT)/src
CFLAGS+=-I. -I$(LWIPDIR)/include -I$(EKALIB_SRCS_DIR) -I./$(EKALIB_SRCS_DIR)/arch -D_GNU_SOURCE

include $(LWIPDIR)/Filelists.mk

LWIPFILES=$(LWIPNOAPPSFILES)  $(EKALIB_SRCS_DIR)/sys_arch.c

LWIPOBJS=$(notdir $(LWIPFILES:.c=.o))

LWIPLIBCOMMON=liblwipcommon.a
#$(LWIPLIBCOMMON): $(LWIPOBJS)
$(LWIPLIBCOMMON): $(LWIPFILES) $(LWIPOBJS)
	$(AR) $(ARFLAGS) $(LWIPLIBCOMMON) $?
#	rm -f $(LWIPOBJS)

#$(LWIPOBJS): %.o: $(LWIPFILES)
$(LWIPOBJS): %.o:
	$(CCDEP) -fPIC $(CFLAGS) $(GCC_FLAGS) -c $<

lwip_depend: .lwip_depend
include .lwip_depend

.lwip_depend: $(LWIPFILES)
	$(CCDEP) $(CFLAGS) $(GCC_FLAGS) -I . -I $(EKALIB_SRCS_DIR) -MM $^ > .lwip_depend || rm -f .lwip_depend

###############################################
$(EKALIB_OBJ): %.o: %.c
	$(CXX) $(GCC_FLAGS) $(CXXFLAGS) -fPIC -c $< $(EKALINE_FLAGS) $(INCL) -o $@


libeka_depend: .libeka_depend
include .libeka_depend

.libeka_depend: $(EKALIB_SRCS)
	$(CXX) $(GCC_FLAGS) $(CXXFLAGS) $(INCL) -MM $^ > .libeka_depend || rm -f .libeka_depend

ekalib: $(LIBEKALINE).so
$(LIBEKALINE).so: $(EKALIB_OBJ) $(SMARTNIC_LIB) $(LWIP_LIB)
	$(CXX) $(EKALIB_OBJ) -shared -L. $(SMARTNIC_LIB) $(LWIP_LIB) -o $@ $(LD_FLAGS) $(LD_LIBS) -Wl,--version-script=$(LIBEKALINE).map


$(LIBEKALINE).a: $(EKALIB_OBJ) $(SMARTNIC_LIB) $(LWIPOBJS)
	$(AR) x $(SMARTNIC_LIB)
	$(AR) rcs $@ $(EKALIB_OBJ) $(SMARTNIC_LIB_FILES) $(LWIPOBJS)

eka_utils: $(EKAUTILS)
$(EKAUTILS): %: %.c $(LIBEKALINE).a
	$(CXX) $(GCC_FLAGS) $(CXXFLAGS) -o $@ $^ $(INCL) $(LD_LIBS) $(EKALINE_FLAGS) $(RUNTIME_LD_FLAGS)

eka_tests: $(EKATESTS_EXECS)
../bc_tests/%: ../bc_tests/%.c $(LIBEKALINE).so
	$(CXX) $(GCC_FLAGS) $(CXXFLAGS) $< -o $@ $(INCL) $(LD_LIBS) $(EKALINE_FLAGS) $(LLIB) $(RUNTIME_LD_FLAGS)

clean:
	rm -f /tmp/core* *.o */*.o *.so *.a *~ core $(EKATESTS_EXECS) $(UNITTESTS_EXECS) $(EKAUTILS_EXECS) */*~ ../*/*~ *.csv *.txt
#	rm -f /tmp/core* *.o */*.o *.so *.a $(LWIPOBJS) *~ .depend core $(EKATESTS_EXECS) $(UNITTESTS_EXECS) $(EKAUTILS_EXECS) */*~ ../*/*~ *.csv *.txt
