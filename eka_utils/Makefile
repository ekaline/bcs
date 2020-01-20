ifeq ($(shell hostname),nqxlxavt188d)
  SMARTNIC_ROOT:=/local/1/sysavtekaline/SmartNIC_SW
#SMARTNIC_ROOT:=/home/stermanj/cboebk/forKon/ekaline3/SmartNIC_SW
else
#  SMARTNIC_ROOT:= $(HOME)/ekatools/smartnic/SmartNIC_SW_3_0_0_RC_Ekaline
  SMARTNIC_ROOT:= $(HOME)/ekatools/smartnic/SmartNIC_SW
endif 

DRIVERS_INCL:=$(SMARTNIC_ROOT)/driver/
API_INCL:=$(SMARTNIC_ROOT)/include/
INC=$(DRIVERS_INCL) $(API_INCL) $(shell pwd) $(shell pwd)/compat
INCL=$(foreach d, $(INC), -I$d)

CC := g++ -std=c++17 -O3

GCC_FLAGS:=-Wall -fpic -pthread -lrt -lm 

EKAUTILS_SRCS := $(wildcard *.c)
EKAUTILS_EXECS := $(patsubst %.c,%,$(EKAUTILS_SRCS))


EKALIB_SRCS := $(wildcard *.c)
EKALIB_FILES := $(patsubst %.c,%,$(EKALIB_SRCS))
EKALIB_OBJECTS := $(EKALIB_SRCS:.c=.o)
LLIB := -L. -lekaline2
SMARTNIC_LIB := $(SMARTNIC_ROOT)/lib/libsmartnic.a

all: $(EKAUTILS_EXECS)
	@echo "========================================="
	@echo created: $^

$(EKAUTILS_EXECS): %: %.c
	$(CC) $< -o $@ $(GCC_FLAGS) -I . -I $(DRIVERS_INCL) -I $(API_INCL) -I ../apisrc $(SMARTNIC_LIB)


clean:
	rm -f *.o *.so *~ core $(EKAUTILS_EXECS) */*~ ../*/*~
