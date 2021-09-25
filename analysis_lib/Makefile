#
# LLVM Directory
#
LLVM_DIR := ../build

CXX := $(LLVM_DIR)/bin/clang++
AR := $(LLVM_DIR)/bin/llvm-ar
RANLIB = $(LLVM_DIR)/bin/llvm-ranlib
CXXFLAGS := -O3 -fPIC

SRC := analysis.cpp
LIB_ANALYSIS := libanalysis

.PHONY: libanalysis clean

#
# Compile to a static library
#
a: $(SRC)
	$(CXX) $(CXXFLAGS) -c $(SRC) -o $(LIB_ANALYSIS).o
	$(AR) -rc $(LIB_ANALYSIS).a $(LIB_ANALYSIS).o
	$(RANLIB) $(LIB_ANALYSIS).a

debug: $(SRC)
	$(CXX) $(CXXFLAGS) -DDEBUG=1 -c $(SRC) -o $(LIB_ANALYSIS).o
	$(AR) -rc $(LIB_ANALYSIS).a $(LIB_ANALYSIS).o
	$(RANLIB) $(LIB_ANALYSIS).a

clean:
	rm -rf *.o *.a *.s *.ll $(LIB_ANALYSIS)
