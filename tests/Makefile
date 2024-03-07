
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

EKA_SW_HOME := $(MAKEFILE_DIR)/..

CXX := g++
CXXFLAGS := -I$(EKA_SW_HOME)/api -I$(EKA_SW_HOME)/tests
LDFLAGS := -L$(EKA_SW_HOME)/lib -lEkaBcs -Wl,-rpath,$(EKA_SW_HOME)/lib

SOURCES := bcsMdTest.cpp bcsTcpConnect.cpp fireReportOnly.cpp

EXECUTABLES := $(SOURCES:.cpp=)

all: $(EXECUTABLES)

$(EXECUTABLES): %: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(EXECUTABLES)
