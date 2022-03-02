TOPDIR := $(shell realpath ..)

GCC_LIBDIR ?= /srv/opt/gcc/gcc-11.2.0/lib64/

TESTS := test_epm_local
#test_epm_remote

INCLUDEDIR += $(TOPDIR)/apisrc
LIBDIR += $(TOPDIR)/apisrc

LIBRARIES += ekaline2 gtest pthread

CXXFLAGS += -ggdb3 -O0 -Wall -Werror -std=c++2x
CFLAGS += -ggdb3 -O0 -Wall -Werror

CFLAGS += $(foreach incdir,$(INCLUDEDIR),-I $(incdir))
CXXFLAGS += $(foreach incdir,$(INCLUDEDIR),-I $(incdir))

LDFLAGS += $(foreach libdir,$(LIBDIR),-L $(libdir))
LDFLAGS += $(foreach library,$(LIBRARIES),-l$(library))

all: dep build test

clean:
	rm -f *.o test_epm_local test_epm_remote

debug-make:
	@echo "TOPDIR = $(TOPDIR)"
	@echo "INCLUDEDIR = $(INCLUDEDIR)"
	@echo "LIBDIR = $(LIBDIR)"
	@echo "LIBRARIES = $(LIBRARIES)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "CXXFLAGS = $(CXXFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"


.PHONY: default all dep build test clean
# TODO(twozniak): depend

build: test_epm_local
#test_epm_remote

test: build
	@echo "Executing gtest tests"
	LD_LIBRARY_PATH=$(TOPDIR)/apisrc:$(GCC_LIBDIR):${LD_LIBRARY_PATH} ./test_epm_local

test_epm_local: test_epm_local.cpp

test_epm_remote:  test_epm_remote.cpp