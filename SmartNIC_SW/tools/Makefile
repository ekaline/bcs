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
# Build Silicom utilities.
#

# Per default STRICT compilation is enabled
ifndef STRICT
    STRICT := 1
endif

# Values of PRODUCT, LOWER_PRODUCT, GNUCSTRICT and GNUCPPSTRICT etc
include ../api/makefile_common

ALL_TARGETS := $(patsubst %.c,%,$(shell ls *.c))
ALL_OBJECTS := $(patsubst %,%.o,$(ALL_TARGETS))

# Common options for c and C++ release and debug builds:
COMMON_CFLAGS := $(COMMON_FLAGS)
COMMON_CFLAGS += -fPIC -Wall -Wextra -Wpointer-arith -fwhole-program
COMMON_CFLAGS += -I../include -I../utils -I../api

# Specific options for C and C++:
CFLAGS := $(COMMON_CFLAGS) $(GNUCSTRICT)
CPPFLAGS := $(COMMON_CFLAGS) $(GNUCPPSTRICT)
CPPFLAGS += -std=c++0x
#CPPFLAGS += -std=c++11

# Release build is the default:
release: CFLAGS += -DNDEBUG -O3
release: CPPFLAGS += -DNDEBUG -O3
ifdef PROFILE
    release: CFLAGS += -fprofile-arcs -ftest-coverage
    release: CPPFLAGS += -fprofile-arcs -ftest-coverage
endif
release: all

# To make a debug build run "make debug"
debug: CFLAGS += -DDEBUG -O0 -g
debug: CPPFLAGS += -DDEBUG -O0 -g
ifdef PROFILE
    debug: CFLAGS += -fprofile-arcs -ftest-coverage
    debug: CPPFLAGS += -fprofile-arcs -ftest-coverage
endif
debug: all

# Select between linking to shared library or static library:
#LIBS := -L../lib -l$(LOWER_PRODUCT)
LIBS := ../lib/lib$(LOWER_PRODUCT).a -lstdc++
LIBS += -lrt -lpthread -lm

all:
	@echo "--- Building tools ---"
	@for i in $(ALL_TARGETS); do echo "$(CC) $$i.c $(CFLAGS) $(LIBS) -o $$i"; $(CC) $$i.c $(CFLAGS) $(LIBS) -o $$i; done

clean:
	rm -f $(ALL_OBJECTS) $(ALL_TARGETS) *.gcno
