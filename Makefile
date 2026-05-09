# =============================================================================
# Makefile — Huffman Coding
# Course : Design and Analysis of Algorithms
# Author : Rheman E. Pasia
#
# Targets:
#   make           → builds the compressor binary (./huffman)
#   make test      → builds and runs the full test suite (./test_all)
#   make clean     → removes all build artefacts
#
# Requirements: GCC 7+ or Clang 5+ with C++17 support.
# =============================================================================

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I include/

SRC_DIR   = src
TEST_DIR  = tests
INC_DIR   = include

CORE_SRC  = $(SRC_DIR)/huffman.cpp
MAIN_SRC  = $(SRC_DIR)/main.cpp
TEST_SRC  = $(TEST_DIR)/test_all.cpp

TARGET    = huffman
TEST_BIN  = test_all

# =============================================================================
# Primary targets
# =============================================================================

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(CORE_SRC) $(MAIN_SRC) $(INC_DIR)/huffman.h
	$(CXX) $(CXXFLAGS) $(CORE_SRC) $(MAIN_SRC) -o $(TARGET)
	@echo "Built: ./$(TARGET)"

$(TEST_BIN): $(CORE_SRC) $(TEST_SRC) $(INC_DIR)/huffman.h
	$(CXX) $(CXXFLAGS) $(CORE_SRC) $(TEST_SRC) -o $(TEST_BIN)
	@echo "Built: ./$(TEST_BIN)"

test: $(TEST_BIN)
	@echo ""
	./$(TEST_BIN)

clean:
	rm -f $(TARGET) $(TEST_BIN) *.huf *_decoded.*
	@echo "Cleaned."

# =============================================================================
# Usage reminder
# =============================================================================
help:
	@echo "Usage:"
	@echo "  make              Build the compressor (./huffman)"
	@echo "  make test         Build and run all test cases (./test_all)"
	@echo "  make clean        Remove binaries and output files"
	@echo ""
	@echo "Compressor usage:"
	@echo "  ./huffman compress   <file>        Compress a file → <file>.huf"
	@echo "  ./huffman decompress <file>.huf    Decompress     → <base>_decoded.<ext>"
