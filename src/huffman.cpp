#include "huffman.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

// =============================================================================
// Internal helpers
// =============================================================================

// ── Bit-level writer ──────────────────────────────────────────────────────
// Accumulates bits into a byte buffer.  When the buffer reaches 8 bits it
// appends one byte to the output vector.  Call flush() after the last bit to
// pad and emit the final (possibly partial) byte.
struct BitWriter {
    std::vector<uint8_t>& out;
    uint8_t  buffer   = 0;
    int      bitsHeld = 0;

    explicit BitWriter(std::vector<uint8_t>& sink) : out(sink) {}

    // Append a single bit (0 or 1) to the stream.  O(1) amortized.
    void writeBit(int bit) {
        buffer = static_cast<uint8_t>((buffer << 1) | (bit & 1));
        if (++bitsHeld == 8) {
            out.push_back(buffer);
            buffer   = 0;
            bitsHeld = 0;
        }
    }

    // Write all bits of a code string (e.g., "0110").
    void writeCode(const std::string& code) {
        for (char c : code) writeBit(c - '0');
    }

    // Flush remaining bits with zero padding; returns the padding count.
    int flush() {
        if (bitsHeld == 0) return 0;
        int pad = 8 - bitsHeld;
        buffer = static_cast<uint8_t>(buffer << pad);
        out.push_back(buffer);
        return pad;
    }
};

// ── Bit-level reader ──────────────────────────────────────────────────────
// Reads one bit at a time from a byte slice, MSB first.
struct BitReader {
    const uint8_t* data;
    size_t         byteLen;
    size_t         bytePos  = 0;
    int            bitPos   = 7;  // current bit within the current byte (7=MSB)

    BitReader(const uint8_t* d, size_t len) : data(d), byteLen(len) {}

    // Returns the next bit (0 or 1), or -1 when the stream is exhausted.
    int nextBit() {
        if (bytePos >= byteLen) return -1;
        int bit = (data[bytePos] >> bitPos) & 1;
        if (--bitPos < 0) {
            ++bytePos;
            bitPos = 7;
        }
        return bit;
    }
};

// ── Little-endian 32-bit write/read ───────────────────────────────────────
static void writeU32LE(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v));
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >> 24));
}
static uint32_t readU32LE(const uint8_t* p) {
    return  static_cast<uint32_t>(p[0])
          | static_cast<uint32_t>(p[1]) << 8
          | static_cast<uint32_t>(p[2]) << 16
          | static_cast<uint32_t>(p[3]) << 24;
}

// ── Recursive code-table builder ──────────────────────────────────────────
static void generateCodes(HuffmanNode* node,
                           const std::string& prefix,
                           CodeTable& table) {
    if (!node) return;

    if (node->isLeaf()) {
        // A single-symbol tree has no branches; guard with "0" so the
        // decoder can still advance the tree pointer.
        table[node->symbol] = prefix.empty() ? "0" : prefix;
        return;
    }

    generateCodes(node->left,  prefix + "0", table);
    generateCodes(node->right, prefix + "1", table);
}

// =============================================================================
// Phase 1 — Frequency Counting   O(n)
// =============================================================================
FreqTable buildFrequencyTable(const std::vector<uint8_t>& input) {
    FreqTable freq{};   // zero-initialised by value-initialisation
    for (uint8_t byte : input)
        ++freq[byte];
    return freq;
}

// =============================================================================
// Phase 2 — Tree Construction    O(m log m)
//
// Algorithm (greedy, bottom-up):
//   1. Insert one leaf node per distinct symbol into a min-heap.
//   2. Repeatedly extract the two minimum-frequency nodes, create a parent
//      whose weight is their sum, and re-insert the parent.
//   3. After m − 1 merges the heap holds one node: the root.
//
// Total operations: 3(m − 1) heap ops, each O(log m)  →  O(m log m).
// For byte-level inputs m ≤ 256, so this is effectively O(1) relative to n.
// =============================================================================
HuffmanNode* buildTree(const FreqTable& freq) {
    MinHeap heap;

    // Insert one leaf per distinct symbol
    for (int sym = 0; sym < 256; ++sym) {
        if (freq[sym] > 0)
            heap.push(new HuffmanNode(static_cast<uint8_t>(sym), freq[sym]));
    }

    if (heap.empty())
        throw std::invalid_argument("buildTree: input is empty.");

    // Edge case: single distinct symbol → no branches exist.
    // Wrap in a parent so the tree always has height ≥ 1.
    if (heap.size() == 1) {
        HuffmanNode* only = heap.top(); heap.pop();
        return new HuffmanNode(only->frequency, only, nullptr);
    }

    // Greedy merging loop (m − 1 iterations)
    while (heap.size() > 1) {
        HuffmanNode* left  = heap.top(); heap.pop();
        HuffmanNode* right = heap.top(); heap.pop();
        heap.push(new HuffmanNode(left->frequency + right->frequency,
                                   left, right));
    }

    return heap.top();  // sole remaining node is the root
}

// =============================================================================
// Phase 3 — Code Generation      O(m)
// Traverses every node in the tree exactly once via DFS.
// Left branches append '0'; right branches append '1'.
// =============================================================================
CodeTable buildCodeTable(HuffmanNode* root) {
    CodeTable table;
    generateCodes(root, "", table);
    return table;
}

// =============================================================================
// Phase 4 — Compression          O(n)
//
// Output layout (see huffman.h for the full format spec):
//   magic (4) | orig_size (4) | m (1) | m×(sym+freq) | padding (1) | bits
// =============================================================================
std::vector<uint8_t> compress(const std::vector<uint8_t>& input) {
    if (input.empty())
        throw std::invalid_argument("compress: input must be non-empty.");

    // ── Phase 1: frequency counting ──────────────────────────────────────
    FreqTable freq = buildFrequencyTable(input);

    // Collect distinct symbols for the header
    std::vector<std::pair<uint8_t, uint32_t>> freqEntries;
    for (int s = 0; s < 256; ++s)
        if (freq[s] > 0)
            freqEntries.push_back({ static_cast<uint8_t>(s),
                                    static_cast<uint32_t>(freq[s]) });

    uint8_t m = static_cast<uint8_t>(
        freqEntries.size() == 256 ? 0 : freqEntries.size());

    // ── Phase 2: tree construction ───────────────────────────────────────
    HuffmanNode* root = buildTree(freq);

    // ── Phase 3: code generation ─────────────────────────────────────────
    CodeTable codes = buildCodeTable(root);
    freeTree(root);

    // ── Build output buffer ───────────────────────────────────────────────
    std::vector<uint8_t> out;
    // Reserve rough estimate to avoid reallocations
    out.reserve(input.size() / 2 + freqEntries.size() * 5 + 16);

    // Magic
    out.insert(out.end(), MAGIC, MAGIC + 4);

    // Original size (4 bytes LE)
    writeU32LE(out, static_cast<uint32_t>(input.size()));

    // Number of distinct symbols (0 encodes 256)
    out.push_back(m);

    // Frequency table entries: 1 byte symbol + 4 bytes freq
    for (auto& [sym, f] : freqEntries) {
        out.push_back(sym);
        writeU32LE(out, f);
    }

    // Reserve one byte for the padding count; fill in after encoding
    size_t paddingIdx = out.size();
    out.push_back(0);

    // ── Phase 4: encoding pass ────────────────────────────────────────────
    BitWriter writer(out);
    for (uint8_t byte : input)
        writer.writeCode(codes.at(byte));

    int pad = writer.flush();
    out[paddingIdx] = static_cast<uint8_t>(pad);

    return out;
}

// =============================================================================
// Phase 5 — Decompression        O(n)
//
// 1. Validate the magic number.
// 2. Read the frequency table and reconstruct the identical Huffman tree.
// 3. Traverse the tree bit-by-bit, emitting a symbol each time a leaf is
//    reached and resetting the current pointer to the root.
// =============================================================================
std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed) {
    if (compressed.size() < HEADER_MIN)
        throw std::runtime_error("decompress: stream too short.");

    const uint8_t* p = compressed.data();
    size_t         pos = 0;

    // ── Validate magic ────────────────────────────────────────────────────
    if (std::memcmp(p, MAGIC, 4) != 0)
        throw std::runtime_error("decompress: invalid magic number.");
    pos += 4;

    // ── Read original size ────────────────────────────────────────────────
    uint32_t origSize = readU32LE(p + pos); pos += 4;

    // ── Read frequency table ──────────────────────────────────────────────
    uint8_t mRaw = p[pos++];
    int     m    = (mRaw == 0) ? 256 : mRaw;

    if (pos + static_cast<size_t>(m) * 5 + 1 > compressed.size())
        throw std::runtime_error("decompress: header truncated.");

    FreqTable freq{};
    for (int i = 0; i < m; ++i) {
        uint8_t  sym = p[pos++];
        uint32_t f   = readU32LE(p + pos); pos += 4;
        freq[sym]    = f;
    }

    // ── Read padding count ────────────────────────────────────────────────
    uint8_t padding = p[pos++];

    // ── Reconstruct Huffman tree (same algorithm as encoding) ────────────
    HuffmanNode* root = buildTree(freq);

    // ── Decode bitstream ──────────────────────────────────────────────────
    size_t  dataLen = compressed.size() - pos;
    BitReader reader(p + pos, dataLen);

    std::vector<uint8_t> output;
    output.reserve(origSize);

    HuffmanNode* current = root;

    // Single-symbol edge case: tree has one leaf with no branches
    if (root->isLeaf()) {
        for (uint32_t i = 0; i < origSize; ++i)
            output.push_back(root->symbol);
        freeTree(root);
        return output;
    }

    // Total bits in the payload = dataLen * 8 - padding
    long long totalBits = static_cast<long long>(dataLen) * 8 - padding;
    long long bitsRead  = 0;

    while (bitsRead < totalBits && output.size() < origSize) {
        int bit = reader.nextBit();
        if (bit < 0) break;
        ++bitsRead;

        current = bit ? current->right : current->left;

        if (current && current->isLeaf()) {
            output.push_back(current->symbol);
            current = root;
        }
    }

    freeTree(root);

    if (output.size() != origSize)
        throw std::runtime_error("decompress: output length mismatch — "
                                 "stream may be corrupted.");

    return output;
}

// =============================================================================
// Memory management
// =============================================================================
void freeTree(HuffmanNode* node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    delete node;
}

// =============================================================================
// Diagnostic utilities
// =============================================================================

// Prints symbols with non-zero frequency in descending order.
void printFrequencyTable(const FreqTable& freq) {
    // Collect and sort by descending frequency
    std::vector<std::pair<uint64_t, uint8_t>> entries;
    for (int i = 0; i < 256; ++i)
        if (freq[i] > 0)
            entries.push_back({ freq[i], static_cast<uint8_t>(i) });
    std::sort(entries.rbegin(), entries.rend());

    std::cout << "  Symbol  ASCII   Frequency\n";
    std::cout << "  ------  -----   ---------\n";
    for (auto& [f, s] : entries) {
        std::cout << "  ";
        if (s >= 32 && s < 127)
            std::cout << std::setw(4) << "'" << static_cast<char>(s) << "'";
        else
            std::cout << std::setw(4) << "\\x" << std::hex
                      << std::setw(2) << std::setfill('0')
                      << static_cast<int>(s) << std::dec << std::setfill(' ');
        std::cout << "  " << std::setw(5) << static_cast<int>(s)
                  << "   " << f << "\n";
    }
}

// Prints codes sorted by ascending code length, then ascending symbol value.
void printCodeTable(const CodeTable& codes, const FreqTable& freq) {
    std::vector<std::tuple<size_t, uint8_t, std::string>> entries;
    for (auto& [sym, code] : codes)
        entries.push_back({ code.size(), sym, code });
    std::sort(entries.begin(), entries.end());

    std::cout << "  Symbol  Code        Length   Freq\n";
    std::cout << "  ------  ----------  ------   ----\n";
    for (auto& [len, sym, code] : entries) {
        std::cout << "  ";
        if (sym >= 32 && sym < 127)
            std::cout << std::setw(4) << "'" << static_cast<char>(sym) << "'";
        else
            std::cout << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(sym) << std::dec << std::setfill(' ');
        std::cout << "  " << std::setw(10) << std::left << code << std::right
                  << "  " << std::setw(4) << len
                  << "   " << freq[sym] << "\n";
    }
}

// Shannon entropy H(S) = -sum( p(s) * log2(p(s)) )
double computeEntropy(const FreqTable& freq, uint64_t total) {
    double H = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] == 0) continue;
        double p = static_cast<double>(freq[i]) / static_cast<double>(total);
        H -= p * std::log2(p);
    }
    return H;
}

// Expected code length L = sum( freq[s]/total * code_length[s] )
double computeAvgCodeLength(const CodeTable& codes,
                            const FreqTable& freq, uint64_t total) {
    double L = 0.0;
    for (auto& [sym, code] : codes)
        L += (static_cast<double>(freq[sym]) / static_cast<double>(total))
             * static_cast<double>(code.size());
    return L;
}
