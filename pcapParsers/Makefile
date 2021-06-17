CC_OPT ?= -O3
CXX_STANDARD ?= c++17
ARCH_FLAGS ?= -march=native
DEBUG_FLAGS ?= -g

GCC_FLAGS ?= $(CC_OPT) $(DEBUG_FLAGS) -frecord-gcc-switches -Wall -pipe $(ARCH_FLAGS)
CXXFLAGS ?= -std=$(CXX_STANDARD)
LD_FLAGS ?= -static-libstdc++ -static-libgcc $(LD_ASAN_OPTIONS)
RUNTIME_LD_FLAGS ?= -static-libstdc++ -static-libgcc
LD_LIBS ?= -pthread -lrt -lm
INCL := -I../apisrc

GCC_SUPPRESSED_WARNINGS:=-Wno-unused-function -Wno-packed-not-aligned

LIBEKA_GIT_VER:= $(shell git rev-parse --short HEAD)

EKALINE_FLAGS := $(GCC_EXTERNAL_DEFINES) -DLIBEKA_GIT_VER=\"$(LIBEKA_GIT_VER)\" $(GCC_SUPPRESSED_WARNINGS)

all: batsPcapParse topoPcapParse xdpPcapParse tomPcapParse nomPcapParse gemPcapParse

batsPcapParse: batsPcapParse.c ../apisrc/EkaFhBatsParser.h
	$(CXX) $(GCC_FLAGS) -o $@ $^ $(INCL) $(RUNTIME_LD_FLAGS) $(EKALINE_FLAGS)

topoPcapParse: topoPcapParse.c ../apisrc/EkaFhPhlxTopoParser.h
	$(CXX) $(GCC_FLAGS) -o $@ $^ $(INCL) $(RUNTIME_LD_FLAGS) $(EKALINE_FLAGS)

xdpPcapParse: xdpPcapParse.c ../apisrc/EkaFhXdpParser.h
	$(CXX) $(GCC_FLAGS) -o $@ $^ $(INCL) $(RUNTIME_LD_FLAGS) $(EKALINE_FLAGS)

tomPcapParse: tomPcapParse.c ../apisrc/EkaFhMiaxParser.h
	$(CXX) $(GCC_FLAGS) -o $@ $^ $(INCL) $(RUNTIME_LD_FLAGS) $(EKALINE_FLAGS)

nomPcapParse: nomPcapParse.c ../apisrc/EkaFhNomParser.h
	$(CXX) $(GCC_FLAGS) -o $@ $^ $(INCL) $(RUNTIME_LD_FLAGS) $(EKALINE_FLAGS)

gemPcapParse: gemPcapParse.c ../apisrc/EkaFhGemParser.h
	$(CXX) $(GCC_FLAGS) -o $@ $^ $(INCL) $(RUNTIME_LD_FLAGS) $(EKALINE_FLAGS)

clean:
	rm -f /tmp/core* *~ batsPcapParse topoPcapParse xdpPcapParse tomPcapParse nomPcapParse gemPcapParse

