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
# Build user space shared and static libraries.
#

# Per default STRICT compilation is enabled
ifndef STRICT
    STRICT := 1
endif

# Values of PRODUCT, LOWER_PRODUCT, GNUCSTRICT and GNUCPPSTRICT etc
include makefile_common

PRODUCT_INCLUDES := \
	$(LOWER_PRODUCT)_internal.h \
	$(LOWER_PRODUCT)_tools.h \
	../include/$(LOWER_PRODUCT).h \
	../include/$(LOWER_PRODUCT)_common.h \
	../include/release_version.h \
	../driver/ctls.h \
	../utils/Statistics.h \
	../utils/Stopwatch.h \
	../utils/SharedMemory.h

PRODUCT_SOURCES := $(shell ls *.cpp)

PRODUCT_OBJECTS := $(patsubst %.cpp,%.o,$(PRODUCT_SOURCES))

OTHER_SOURCES := \
	../TinyThread++-1.1/tinythread.cpp

OTHER_OBJECTS := $(notdir $(OTHER_SOURCES))
OTHER_OBJECTS := $(patsubst %.cpp,%.o,$(OTHER_OBJECTS))

# Library compilation options:
#   SUPPORT_TX_FLOW_CONTROL_STATISTICS enables/disables collection of Tx flow control statistics.
#   SUPPORT_LOGGING enables/disables support for logging.
#   USE_SPINLOCK chooses between spinlock or mutex
# Default values below can be overridden from make command line or with an environment variable of same name.
# Examples:
#   make SUPPORT_TX_FLOW_CONTROL_STATISTICS=1 # Include Tx flow control statistics in a release build
#   make debug SUPPORT_TX_FLOW_CONTROL_STATISTICS=0 # Exclude Tx flow control statistics from a debug build

# Release build is the default target
release:

ifndef SUPPORT_TX_FLOW_CONTROL_STATISTICS
    # Default value if not defined:
    release: SUPPORT_TX_FLOW_CONTROL_STATISTICS := 0
    debug:   SUPPORT_TX_FLOW_CONTROL_STATISTICS := 1
endif

ifndef SUPPORT_LOGGING
    # Default value if not defined:
    release: SUPPORT_LOGGING := 0
    debug:   SUPPORT_LOGGING := 1
endif

ifndef USE_SPINLOCK
    # Default value if not defined:
    release: USE_SPINLOCK := 1
    debug:   USE_SPINLOCK := 1
endif

ifndef SANITY_CHECKS
    # Default value if not defined:
    release: SANITY_CHECKS := 0
    debug:   SANITY_CHECKS := 1
endif

ifndef API_TIMING
    # Default value if not defined:
    release: API_TIMING := 0
    debug:   API_TIMING := 0
endif

# Common options for c and c++:
COMMON_CFLAGS := $(COMMON_FLAGS)
COMMON_CFLAGS += -fPIC -m64 -Wall -Wextra -Wpointer-arith
COMMON_CFLAGS += -I../include -I../TinyThread++-1.1/ -I../utils -I../driver
COMMON_CFLAGS += -DSUPPORT_TX_FLOW_CONTROL_STATISTICS=$(SUPPORT_TX_FLOW_CONTROL_STATISTICS)
COMMON_CFLAGS += -DSUPPORT_LOGGING=$(SUPPORT_LOGGING)
COMMON_CFLAGS += -DUSE_SPINLOCK=$(USE_SPINLOCK)
COMMON_CFLAGS += -DSANITY_CHECKS=$(SANITY_CHECKS)
COMMON_CFLAGS += -DAPI_TIMING=$(API_TIMING)

# Release and debug build common options:
CPPFLAGS := $(COMMON_CFLAGS) -Wno-unknown-pragmas
CPPFLAGS += $(COMMON_CFLAGS) $(CPP_STANDARD)

CPP := $(CCACHE) /usr/bin/g++
#CPP := $(CCACHE) g++
#CPP := $(CCACHE) clang

# To make a release build run "make" or "make release"
release: CPPFLAGS += -O3 -DNDEBUG
ifdef VALGRIND_PROFILE
    release: CPPFLAGS += -g
endif
release: libraries

# To make a debug build run "make debug"
debug: CPPFLAGS += $(GCC_DEBUG_OPTIMIZE) -g -DDEBUG
ifdef VALGRIND_PROFILE
    #debug: CPPFLAGS += -g
endif
debug: libraries

INCLUDES := -I../TinyThread++-1.1/ -I../utils/ -I../include/ -I../driver

LIBS :=  -lpthread -lm -lrt

# This is what we are building, shared library and static library:
LIBRARIES := ../lib/lib$(LOWER_PRODUCT).so ../lib/lib$(LOWER_PRODUCT).a
libraries: $(LIBRARIES)

../lib/lib$(LOWER_PRODUCT).so: links $(PRODUCT_SOURCES) $(OTHER_SOURCES) $(PRODUCT_INCLUDES)
	@echo
	@echo --- Building shared API library ---
	mkdir -p ../lib
#	$(CPP) -E $(CPPFLAGS) $(PROCUCT_SOURCES) $(OTHER_SOURCES) >packetmover.output
#	$(CPP) -S $(CPPFLAGS) $(PRODUCT_SOURCES) $(OTHER_SOURCES)
	$(CPP) -shared $(CPPFLAGS) $(PRODUCT_SOURCES) $(OTHER_SOURCES) $(LIBS) -o ../lib/lib$(LOWER_PRODUCT).so

../lib/lib$(LOWER_PRODUCT).a: links $(PRODUCT_SOURCES) $(OTHER_SOURCES) $(PRODUCT_INCLUDES) $(OTHER_OBJECTS)
	@echo
	@echo --- Building static API library ---
	mkdir -p ../lib
	$(CPP) $(CPPFLAGS) $(GNUCPPSTRICT) -c $(PRODUCT_SOURCES)
	ar rcs ../lib/lib$(LOWER_PRODUCT).a $(PRODUCT_OBJECTS) $(OTHER_OBJECTS)

# Cannot compile TinyThreads with STRICT options, too many warnings!
tinythread.o: ../TinyThread++-1.1/tinythread.cpp ../TinyThread++-1.1/tinythread.h
	$(CPP) $(CPPFLAGS) -c ../TinyThread++-1.1/tinythread.cpp

clean:
	rm -f *.o *.gcno *~ *.s *.output $(LIBRARIES)

links:
	# These links are only used for Silicom Denmark internal development environment:
	@echo "SUPPORT_TX_FLOW_CONTROL_STATISTICS=$(SUPPORT_TX_FLOW_CONTROL_STATISTICS) SUPPORT_LOGGING=$(SUPPORT_LOGGING) SANITY_CHECKS=$(SANITY_CHECKS) USE_SPINLOCK=$(USE_SPINLOCK) STRICT=$(STRICT) API_TIMING=$(API_TIMING) GCCVERSION=$(GCCVERSION) VALGRIND_PROFILE=$(VALGRIND_PROFILE)"
	@if [ -d ../subrepositories ]; then \
	    if [ -e ../subrepositories/code_and_scripts ]; then \
	        if [ -h ../subrepositories/code_and_scripts ]; then \
	            unlink ../subrepositories/code_and_scripts; \
	            ln -sf ../subrepositories/SmartNIC/subrepositories/code_and_scripts ../subrepositories/code_and_scripts; \
	        fi; \
	    else \
	        ln -sf ../subrepositories/SmartNIC/subrepositories/code_and_scripts ../subrepositories/code_and_scripts; \
	    fi; \
	    if [ ! -e ../TinyThread++-1.1 ]; then rm -rf ../TinyThread++-1.1; ln -sf ./subrepositories/code_and_scripts/TinyThread++-1.1/source ../TinyThread++-1.1; fi; \
	    if [ ! -e ../utils ]; then rm -rf ../utils; ln -sf ./subrepositories/code_and_scripts/utils ../utils; fi; \
	fi
