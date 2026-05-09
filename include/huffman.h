#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

// =============================================================================
// Compressed-stream binary format
// -----------------------------------------------------------------------------
// Offset  Size        Field
// ------  ----------  ---------------------------------------------------------
//  0       4 bytes    Magic number  0x48 0x55 0x46 0x46  ("HUFF")
//  4       4 bytes    original_size — number of bytes in the original input
//  8       1 byte     m — number of distinct symbols (0 encodes 256)
//  9       m × 5 B    Frequency table entries: symbol (1 B) + freq (4 B LE)
//  9+5m    1 byte     padding_bits — zero-bits appended to fill the last byte
//  10+5m   ...        Encoded bitstream
// =============================================================================

// ── Compressed-format constants ───────────────────────────────────────────
constexpr uint8_t  MAGIC[4]   = { 0x48, 0x55, 0x46, 0x46 }; // "HUFF"
constexpr uint32_t HEADER_MIN = 4 + 4 + 1 + 1;              // magic+orig+m+pad

// =============================================================================
// HuffmanNode
// Each node is either a leaf (representing one input symbol) or an internal
// node (representing a merged pair).  Leaf nodes are identified by the
// isLeaf() predicate (both child pointers are null).
// =============================================================================
struct HuffmanNode {
    uint8_t      symbol;     // Valid for leaf nodes only
    uint64_t     frequency;  // Occurrence count (leaf) or child-sum (internal)
    HuffmanNode* left;
    HuffmanNode* right;

    // Leaf constructor
    HuffmanNode(uint8_t sym, uint64_t freq)
        : symbol(sym), frequency(freq), left(nullptr), right(nullptr) {}

    // Internal-node constructor
    HuffmanNode(uint64_t freq, HuffmanNode* l, HuffmanNode* r)
        : symbol(0), frequency(freq), left(l), right(r) {}

    // A node is a leaf when it has no children
    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

// =============================================================================
// NodeComparator
// Provides a min-heap ordering for the STL priority_queue: the node with the
// LOWEST frequency has the HIGHEST priority (is extracted first).
// =============================================================================
struct NodeComparator {
    bool operator()(const HuffmanNode* a, const HuffmanNode* b) const {
        return a->frequency > b->frequency; // '>' inverts default max-heap
    }
};

// =============================================================================
// Type aliases
// =============================================================================
using MinHeap   = std::priority_queue<HuffmanNode*,
                                       std::vector<HuffmanNode*>,
                                       NodeComparator>;
using FreqTable = std::array<uint64_t, 256>;   // freq[byte_value] = count
using CodeTable = std::unordered_map<uint8_t, std::string>; // sym -> "0110..."

// =============================================================================
// Phase 1 — Frequency Counting   O(n)
// =============================================================================
FreqTable buildFrequencyTable(const std::vector<uint8_t>& input);

// =============================================================================
// Phase 2 — Tree Construction    O(m log m)
// Returns a pointer to the root of the completed Huffman tree.
// The caller is responsible for freeing the tree with freeTree().
// =============================================================================
HuffmanNode* buildTree(const FreqTable& freq);

// =============================================================================
// Phase 3 — Code Generation      O(m)
// Traverses the tree and populates a CodeTable mapping each symbol to its
// variable-length binary code string (e.g., 'a' -> "0", 'b' -> "110").
// =============================================================================
CodeTable buildCodeTable(HuffmanNode* root);

// =============================================================================
// Phase 4 — Compression          O(n)
// Writes the HUFF header followed by the encoded bitstream.
// Returns the complete compressed byte sequence.
// =============================================================================
std::vector<uint8_t> compress(const std::vector<uint8_t>& input);

// =============================================================================
// Phase 5 — Decompression        O(n)
// Reads a HUFF-format byte sequence, reconstructs the tree from the header,
// and traverses it bit-by-bit to recover the original symbol sequence.
// =============================================================================
std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed);

// =============================================================================
// Memory management
// =============================================================================
void freeTree(HuffmanNode* node);

// =============================================================================
// Diagnostic / display utilities (used by test_all.cpp)
// =============================================================================
void printFrequencyTable(const FreqTable& freq);
void printCodeTable(const CodeTable& codes, const FreqTable& freq);
double computeEntropy(const FreqTable& freq, uint64_t total);
double computeAvgCodeLength(const CodeTable& codes,
                            const FreqTable& freq, uint64_t total);
