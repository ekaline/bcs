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
# Build Silicom Flash Library shared and static libraries.
#

#FL_FLASH_LIBRARY_CFLAGS = -Wall -D_GNU_SOURCE -DUSE_FL_FLASH_LIBRARY -O0 -flto -I../../subrepositories/fbcommon/include -I../../include -I..
release: FL_FLASH_LIBRARY_EXTRA_CFLAGS = -O3
debug:   FL_FLASH_LIBRARY_EXTRA_CFLAGS = -O0
FL_FLASH_LIBRARY_CFLAGS = $(FL_FLASH_LIBRARY_EXTRA_CFLAGS) -Wall -Wno-unknown-pragmas -D_GNU_SOURCE -DUSE_FL_FLASH_LIBRARY -I../../subrepositories/fbcommon/include -I../../include -I..
FL_FLASH_LIBRARY_CC = cc

FL_FLASH_LIBRARY_INCLUDES = ../../include/FL_FlashLibrary.h
FL_FLASH_LIBRARY_SOURCES := FL_FlashLibrary.c
FL_FLASH_LIBRARY_OBJECTS := $(patsubst %.c,%.o,$(FL_FLASH_LIBRARY_SOURCES))
FL_FLASH_LIBRARY_LIB_PATH := ../../lib
FL_FLASH_LIBRARY_LIBS := $(FL_FLASH_LIBRARY_LIB_PATH)/FL_FlashLibrary.so $(FL_FLASH_LIBRARY_LIB_PATH)/FL_FlashLibrary.a

release: fl_flash_library_libs #unit_tests
debug: fl_flash_library_libs unit_tests

fl_flash_library_libs: $(FL_FLASH_LIBRARY_LIBS)

../../lib/FL_FlashLibrary.so: $(FL_FLASH_LIBRARY_SOURCES) $(FL_FLASH_LIBRARY_INCLUDES)
	@echo
	@echo --- Building shared flashing library ---
	mkdir -p $(FL_FLASH_LIBRARY_LIB_PATH)
#	$(FL_FLASH_LIBRARY_CC) -E $(FL_FLASH_LIBRARY_CFLAGS) $(FL_FLASH_LIBRARY_SOURCES) >FL_FlashLibrary.output
	$(FL_FLASH_LIBRARY_CC) -shared -fPIC $(FL_FLASH_LIBRARY_CFLAGS) $(FL_FLASH_LIBRARY_SOURCES) -o $(FL_FLASH_LIBRARY_LIB_PATH)/FL_FlashLibrary.so

../../lib/FL_FlashLibrary.a: $(FL_FLASH_LIBRARY_SOURCES) $(FL_FLASH_LIBRARY_INCLUDES)
	@echo
	@echo --- Building static flashing library ---
	mkdir -p $(FL_FLASH_LIBRARY_LIB_PATH)
	$(FL_FLASH_LIBRARY_CC) -E $(FL_FLASH_LIBRARY_CFLAGS) $(FL_FLASH_LIBRARY_SOURCES) >FL_FlashLibrary.output
#	$(FL_FLASH_LIBRARY_CC) -S $(FL_FLASH_LIBRARY_CFLAGS) $(FL_FLASH_LIBRARY_SOURCES)
	$(FL_FLASH_LIBRARY_CC) $(FL_FLASH_LIBRARY_CFLAGS) $(GNUCPPSTRICT) -c $(FL_FLASH_LIBRARY_SOURCES)
	ar rcs $(FL_FLASH_LIBRARY_LIB_PATH)/FL_FlashLibrary.a $(FL_FLASH_LIBRARY_OBJECTS)

unit_tests:
	@if [ -f FL_FlashLibraryUnitTests.cpp ]; then g++ -std=c++11 $(FL_FLASH_LIBRARY_CFLAGS) $(FL_FLASH_LIBRARY_EXTRA_CFLAGS) FL_FlashLibraryUnitTests.cpp ../printlog.c ../../lib/FL_FlashLibrary.a -o UnitTests; fi

clean:
	@rm -f $(FL_FLASH_LIBRARY_LIBS) *.o

