TOPDIR := $(shell realpath ..)

BUILD_DATE := \"$(shell date +%y%m%d_%H%M%S)\"
GIT_REVISION := \"$(shell git log -1 --pretty=oneline | cut -c1-8)\"

GCC_LIBDIR ?= /srv/opt/gcc/gcc-11.2.0/lib64/

TESTS := test_epm_local
#test_epm_remote

INCLUDEDIR += $(TOPDIR)/apisrc
LIBDIR += $(TOPDIR)/apisrc

LIBRARIES += ekaline2 gtest pthread

CXXFLAGS += -ggdb3 -O0 -Wall -Werror -std=c++20 -DBUILD_DATE=$(BUILD_DATE) -DGIT_REVISION=$(GIT_REVISION)
CFLAGS += -ggdb3 -O0 -Wall -Werror  -DBUILD_DATE=$(BUILD_DATE) -DGIT_REVISION=$(GIT_REVISION)

CFLAGS += $(foreach incdir,$(INCLUDEDIR),-I $(incdir))
CXXFLAGS += $(foreach incdir,$(INCLUDEDIR),-I $(incdir))

LDFLAGS += $(foreach libdir,$(LIBDIR),-L $(libdir))
LDFLAGS += $(foreach library,$(LIBRARIES),-l$(library))

all: dep build test

clean:
	rm -f *.o test_epm_local test_epm_remote

debug-make:
	@echo "TOPDIR = $(TOPDIR)"
	@echo "BUILD_DATE = $(BUILD_DATE)"
	@echo "GIT_REVISION = $(GIT_REVISION)"
	@echo "INCLUDEDIR = $(INCLUDEDIR)"
	@echo "LIBDIR = $(LIBDIR)"
	@echo "LIBRARIES = $(LIBRARIES)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "CXXFLAGS = $(CXXFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"


.PHONY: default all dep build test clean
# TODO(twozniak): depend

build: test_epm_local Makefile
#test_epm_remote

test: build
	@echo "Executing gtest tests"
	LD_LIBRARY_PATH=$(TOPDIR)/apisrc:$(GCC_LIBDIR):${LD_LIBRARY_PATH} ./test_epm_local

test_epm_local.o: test_epm_local.cpp common.h Action.h Strategy.h StrategyManager.h Trigger.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test_epm_local: test_epm_local.o
	$(CXX) $(LDFLAGS) -o $@ $<

#test_epm_remote:  test_epm_remote.cpp