#include "huffman.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Formatting helpers
// =============================================================================

static void rule(char ch = '=', int width = 72) {
    std::cout << std::string(width, ch) << "\n";
}

static std::string fmtBytes(size_t n) {
    if (n < 1024)
        return std::to_string(n) + " B";
    if (n < 1048576)
        return std::to_string(n / 1024) + " KB";
    return std::to_string(n / 1048576) + " MB";
}

// Right-aligned column entry
static std::string col(const std::string& s, int w) {
    return std::string(w > static_cast<int>(s.size())
                       ? w - static_cast<int>(s.size()) : 0, ' ') + s;
}

// =============================================================================
// TestResult — collects all metrics for one test case
// =============================================================================
struct TestResult {
    std::string id;
    std::string description;
    size_t inputBytes;
    size_t outputBytes;
    size_t headerBytes;
    size_t dataBytes;
    int    distinctSymbols;
    double avgCodeLen;      // bits/symbol
    double entropy;         // bits/symbol
    double compressionRatio;// output/input * 100
    double encodeMicros;
    double decodeMicros;
    double encodeThroughput;// MB/s
    double decodeThroughput;// MB/s
    bool   roundTripPass;
};

// =============================================================================
// runTest — executes one complete test case
// =============================================================================
static TestResult runTest(const std::string& id,
                          const std::string& description,
                          const std::vector<uint8_t>& input,
                          bool printDetail = true) {
    TestResult r;
    r.id          = id;
    r.description = description;
    r.inputBytes  = input.size();

    // ── Build frequency / code tables for diagnostics ─────────────────────
    FreqTable freq = buildFrequencyTable(input);
    HuffmanNode* root = buildTree(freq);
    CodeTable    codes = buildCodeTable(root);
    freeTree(root);

    r.distinctSymbols = 0;
    for (int i = 0; i < 256; ++i)
        if (freq[i] > 0) ++r.distinctSymbols;

    uint64_t total = static_cast<uint64_t>(input.size());
    r.entropy    = computeEntropy(freq, total);
    r.avgCodeLen = computeAvgCodeLength(codes, freq, total);

    // Header size = magic(4) + orig_size(4) + m(1) + m×5 + padding(1)
    r.headerBytes = 4 + 4 + 1 + static_cast<size_t>(r.distinctSymbols) * 5 + 1;

    // ── Encode (averaged over RUNS) ───────────────────────────────────────
    constexpr int RUNS = 10;
    std::vector<uint8_t> compressed;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int run = 0; run < RUNS; ++run)
        compressed = compress(input);
    auto t1 = std::chrono::high_resolution_clock::now();

    r.encodeMicros = std::chrono::duration<double, std::micro>(t1 - t0).count()
                     / RUNS;
    r.outputBytes  = compressed.size();
    r.dataBytes    = r.outputBytes - r.headerBytes;

    // ── Decode (averaged over RUNS) ───────────────────────────────────────
    std::vector<uint8_t> decoded;

    auto t2 = std::chrono::high_resolution_clock::now();
    for (int run = 0; run < RUNS; ++run)
        decoded = decompress(compressed);
    auto t3 = std::chrono::high_resolution_clock::now();

    r.decodeMicros = std::chrono::duration<double, std::micro>(t3 - t2).count()
                     / RUNS;

    // ── Verify round-trip correctness ─────────────────────────────────────
    r.roundTripPass = (decoded == input);

    // ── Derived metrics ───────────────────────────────────────────────────
    r.compressionRatio  = 100.0 * static_cast<double>(r.outputBytes)
                                / static_cast<double>(r.inputBytes);
    double inputMB      = static_cast<double>(r.inputBytes) / 1048576.0;
    double outputMB     = static_cast<double>(r.outputBytes) / 1048576.0;
    r.encodeThroughput  = inputMB  / (r.encodeMicros / 1e6);
    r.decodeThroughput  = outputMB / (r.decodeMicros / 1e6);

    // ── Print detailed report ─────────────────────────────────────────────
    if (printDetail) {
        rule();
        std::cout << id << " — " << description << "\n";
        rule();

        // Frequency table (only for short inputs to keep output readable)
        if (r.distinctSymbols <= 27) {
            std::cout << "\nFrequency Table:\n";
            printFrequencyTable(freq);
        }

        // Code table (only for short inputs)
        if (r.distinctSymbols <= 27) {
            std::cout << "\nHuffman Codes:\n";
            printCodeTable(codes, freq);
        }

        std::cout << "\nInformation-theoretic metrics:\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  Source entropy H(S)      : " << r.entropy    << " bits/symbol\n";
        std::cout << "  Avg Huffman code length  : " << r.avgCodeLen << " bits/symbol\n";
        std::cout << "  Redundancy (L - H)       : " << r.avgCodeLen - r.entropy
                  << " bits/symbol  [theoretical max: 1.0]\n";

        std::cout << "\nCompression results:\n";
        std::cout << "  Input size               : " << r.inputBytes   << " bytes\n";
        std::cout << "  Distinct symbols (m)     : " << r.distinctSymbols << "\n";
        std::cout << "  Header size              : " << r.headerBytes  << " bytes\n";
        std::cout << "  Encoded data             : " << r.dataBytes    << " bytes\n";
        std::cout << "  Total output             : " << r.outputBytes  << " bytes\n";
        std::cout << std::setprecision(1);
        std::cout << "  Compression ratio (CR)   : " << r.compressionRatio << "%";
        double savings = 100.0 - r.compressionRatio;
        if (savings > 0)
            std::cout << "  (" << savings << "% saved)\n";
        else
            std::cout << "  (+" << -savings << "% expansion)\n";

        std::cout << "\nPerformance (averaged over " << RUNS << " runs):\n";
        std::cout << std::setprecision(2);
        std::cout << "  Encode time              : " << r.encodeMicros << " us\n";
        std::cout << "  Decode time              : " << r.decodeMicros << " us\n";
        std::cout << std::setprecision(1);
        if (r.encodeThroughput > 1.0)
            std::cout << "  Encode throughput        : " << r.encodeThroughput << " MB/s\n";
        else
            std::cout << "  Encode throughput        : (input too small to measure accurately)\n";
        if (r.decodeThroughput > 1.0)
            std::cout << "  Decode throughput        : " << r.decodeThroughput << " MB/s\n";
        else
            std::cout << "  Decode throughput        : (input too small to measure accurately)\n";

        std::cout << "\nRound-trip verification    : "
                  << (r.roundTripPass ? "PASS" : "FAIL") << "\n";
        std::cout << "\n";
    }

    return r;
}

// =============================================================================
// printSummaryTable — reproduces Table VIII-7 and Table VIII-8 from the paper
// =============================================================================
static void printSummaryTable(const std::vector<TestResult>& results) {
    rule('=');
    std::cout << "SUMMARY TABLE\n";
    rule('=');

    // ── Compression summary ───────────────────────────────────────────────
    std::cout << "\nCompression Results  (matches Table VIII-7 in the paper)\n";
    rule('-');
    std::cout
        << std::left
        << std::setw(5)  << "TC"
        << std::setw(28) << "Description"
        << std::right
        << std::setw(10) << "Input"
        << std::setw(5)  << "m"
        << std::setw(10) << "Output"
        << std::setw(9)  << "CR (%)"
        << std::setw(12) << "Savings"
        << "  Pass?\n";
    rule('-');

    for (auto& r : results) {
        double savings = 100.0 - r.compressionRatio;
        std::cout << std::left
                  << std::setw(5)  << r.id
                  << std::setw(28) << r.description
                  << std::right
                  << std::setw(10) << fmtBytes(r.inputBytes)
                  << std::setw(5)  << r.distinctSymbols
                  << std::setw(10) << fmtBytes(r.outputBytes)
                  << std::setw(8)  << std::fixed << std::setprecision(1)
                  << r.compressionRatio << "%";

        if (savings > 0)
            std::cout << std::setw(11) << (std::to_string(static_cast<int>(savings)) + "% saved");
        else
            std::cout << std::setw(11) << ("+" + std::to_string(static_cast<int>(-savings)) + "% exp.");

        std::cout << "  " << (r.roundTripPass ? "PASS" : "FAIL") << "\n";
    }
    rule('-');

    // ── Performance summary ───────────────────────────────────────────────
    std::cout << "\nPerformance Results  (matches Table VIII-8 / VIII-9 in the paper)\n";
    rule('-');
    std::cout
        << std::left
        << std::setw(5)  << "TC"
        << std::setw(28) << "Description"
        << std::right
        << std::setw(10) << "Input"
        << std::setw(13) << "Encode (us)"
        << std::setw(13) << "Decode (us)"
        << std::setw(13) << "Enc MB/s"
        << std::setw(13) << "Dec MB/s\n";
    rule('-');

    for (auto& r : results) {
        std::cout << std::left
                  << std::setw(5)  << r.id
                  << std::setw(28) << r.description
                  << std::right
                  << std::setw(10) << fmtBytes(r.inputBytes)
                  << std::setw(12) << std::fixed << std::setprecision(1)
                  << r.encodeMicros
                  << std::setw(13) << r.decodeMicros;

        if (r.encodeThroughput > 1.0)
            std::cout << std::setw(13) << r.encodeThroughput;
        else
            std::cout << std::setw(13) << "< 1";

        if (r.decodeThroughput > 1.0)
            std::cout << std::setw(13) << r.decodeThroughput;
        else
            std::cout << std::setw(13) << "< 1";

        std::cout << "\n";
    }
    rule('-');
    std::cout << "\n";
}

// =============================================================================
// Text generators for medium and large test cases
// =============================================================================

// Builds a body of English-like prose by cycling through a pool of sentences
// until the target byte count is reached.
static std::vector<uint8_t> generateText(size_t targetBytes) {
    // A pool of varied sentences to give a realistic symbol distribution.
    static const char* SENTENCES[] = {
        "The Huffman algorithm constructs an optimal prefix-free binary code from a set of symbol frequencies. ",
        "In information theory, entropy measures the minimum average number of bits required to encode a source. ",
        "Data compression reduces the number of bits needed to represent information without compromising its integrity. ",
        "The greedy merging strategy ensures that the two least-frequent symbols always appear at maximum depth in the tree. ",
        "A priority queue implemented as a binary min-heap supports insertion and extraction in O(log m) time. ",
        "Prefix-free codes allow unambiguous decoding without delimiters because no codeword is a prefix of another. ",
        "The DEFLATE algorithm combines LZ77 sliding-window compression with Huffman entropy coding. ",
        "Arithmetic coding represents an entire message as a single real number and achieves near-entropy compression. ",
        "The Burrows-Wheeler Transform permutes its input so that similar characters are clustered together. ",
        "Run-length encoding is effective only on inputs containing long sequences of repeated symbols. ",
        "Asymmetric Numeral Systems combine the speed of Huffman coding with the compression ratio of arithmetic coding. ",
        "For byte-level inputs the number of distinct symbols m is bounded by 256, making tree construction O(1) relative to input size. ",
        "The encoded bitstream is padded with zero bits to align to a byte boundary; the padding count is stored in the header. ",
        "Lossless compression guarantees exact reconstruction of the original data without any loss of information. ",
        "The source entropy H(S) provides the theoretical lower bound on the average code length for any lossless scheme. ",
    };
    constexpr int POOL_SIZE = 15;

    std::vector<uint8_t> buf;
    buf.reserve(targetBytes + 256);
    int i = 0;
    while (buf.size() < targetBytes) {
        const char* s = SENTENCES[i % POOL_SIZE];
        while (*s && buf.size() < targetBytes)
            buf.push_back(static_cast<uint8_t>(*s++));
        ++i;
    }
    buf.resize(targetBytes);
    return buf;
}

// TC-R: 900 'a', 50 'b', 30 'c', 22 'd', 12 'e', 10 'f' = 1024 bytes.
static std::vector<uint8_t> generateRepetitive() {
    std::vector<uint8_t> buf;
    buf.reserve(1024);
    for (int i = 0; i < 900; ++i) buf.push_back('a');
    for (int i = 0; i < 50;  ++i) buf.push_back('b');
    for (int i = 0; i < 30;  ++i) buf.push_back('c');
    for (int i = 0; i < 22;  ++i) buf.push_back('d');
    for (int i = 0; i < 12;  ++i) buf.push_back('e');
    for (int i = 0; i < 10;  ++i) buf.push_back('f');
    // Shuffle so the bytes aren't in one big block (tests the encoder, not just the freq counter)
    // Use a simple deterministic shuffle (Fisher-Yates with seed 42)
    uint32_t rng = 42;
    for (size_t i = buf.size() - 1; i > 0; --i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; // xorshift32
        size_t j = rng % (i + 1);
        std::swap(buf[i], buf[j]);
    }
    return buf;
}

// TC-U: every byte value 0-255 appears exactly 4 times = 1024 bytes.
static std::vector<uint8_t> generateUniform() {
    std::vector<uint8_t> buf;
    buf.reserve(1024);
    for (int rep = 0; rep < 4; ++rep)
        for (int v = 0; v < 256; ++v)
            buf.push_back(static_cast<uint8_t>(v));
    // Shuffle with same xorshift32
    uint32_t rng = 99;
    for (size_t i = buf.size() - 1; i > 0; --i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        size_t j = rng % (i + 1);
        std::swap(buf[i], buf[j]);
    }
    return buf;
}

// Convert a string literal to a byte vector.
static std::vector<uint8_t> toBytes(const char* s) {
    std::vector<uint8_t> v;
    while (*s) v.push_back(static_cast<uint8_t>(*s++));
    return v;
}

// =============================================================================
// main
// =============================================================================
int main() {
    std::cout << "\n";
    rule('=');
    std::cout << "Huffman Coding — Full Test Suite\n";
    std::cout << "Course: Design and Analysis of Algorithms\n";
    std::cout << "All nine test cases from Section VIII of the project paper\n";
    rule('=');
    std::cout << "\n";

    std::vector<TestResult> results;

    // ── TC1: "hello" (5 bytes) ─────────────────────────────────────────────
    results.push_back(runTest(
        "TC1",
        "\"hello\" (5 B)",
        toBytes("hello")
    ));

    // ── TC2: "abracadabra" (11 bytes) ─────────────────────────────────────
    // This is the running example from Section V.  The expected codes are:
    //   a → 0 (freq 5)   b → 100 (freq 2)   r → 101 (freq 2)
    //   c → 110 (freq 1)  d → 111 (freq 1)
    // Expected encoded bits: 5+6+6+3+3 = 23 bits → 3 bytes data.
    results.push_back(runTest(
        "TC2",
        "\"abracadabra\" (11 B)",
        toBytes("abracadabra")
    ));

    // ── TC3: pangram sentence (44 bytes) ──────────────────────────────────
    results.push_back(runTest(
        "TC3",
        "Pangram (44 B)",
        toBytes("the quick brown fox jumps over the lazy dog")
    ));

    // ── TC4: short paragraph (512 bytes) ──────────────────────────────────
    results.push_back(runTest(
        "TC4",
        "Short paragraph (512 B)",
        generateText(512)
    ));

    // ── TC5: article excerpt (2,048 bytes) ────────────────────────────────
    results.push_back(runTest(
        "TC5",
        "Article excerpt (2 KB)",
        generateText(2048)
    ));

    // ── TC6: long article (16,384 bytes) ──────────────────────────────────
    results.push_back(runTest(
        "TC6",
        "Long article (16 KB)",
        generateText(16384),
        false   // suppress per-symbol detail for large inputs
    ));
    // Print condensed header for TC6
    {
        auto& r = results.back();
        rule();
        std::cout << "TC6 — Long article (16 KB)\n";
        rule();
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  Input / Output  : " << r.inputBytes << " B  →  " << r.outputBytes << " B\n";
        std::cout << "  Header / Data   : " << r.headerBytes << " B  /  " << r.dataBytes << " B\n";
        std::cout << "  Distinct symbols: " << r.distinctSymbols << "\n";
        std::cout << "  Entropy H(S)    : " << r.entropy    << " bits/symbol\n";
        std::cout << "  Avg code length : " << r.avgCodeLen << " bits/symbol\n";
        std::cout << std::setprecision(1);
        std::cout << "  CR              : " << r.compressionRatio << "%  ("
                  << 100.0 - r.compressionRatio << "% saved)\n";
        std::cout << std::setprecision(2);
        std::cout << "  Encode / Decode : " << r.encodeMicros << " us  /  " << r.decodeMicros << " us\n";
        std::cout << std::setprecision(1);
        std::cout << "  Throughput      : " << r.encodeThroughput << " MB/s enc  |  "
                  << r.decodeThroughput << " MB/s dec\n";
        std::cout << "  Round-trip      : " << (r.roundTripPass ? "PASS" : "FAIL") << "\n\n";
    }

    // ── TC7: full document (1,048,576 bytes = 1 MB) ───────────────────────
    std::cout << "TC7 — Building 1 MB test input...";
    std::cout.flush();
    auto tc7Input = generateText(1048576);
    std::cout << " done.\n";

    results.push_back(runTest(
        "TC7",
        "Full document (1 MB)",
        tc7Input,
        false
    ));
    {
        auto& r = results.back();
        rule();
        std::cout << "TC7 — Full document (1 MB)\n";
        rule();
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  Input / Output  : " << r.inputBytes << " B  →  " << r.outputBytes << " B\n";
        std::cout << "  Header / Data   : " << r.headerBytes << " B  /  " << r.dataBytes << " B\n";
        std::cout << "  Distinct symbols: " << r.distinctSymbols << "\n";
        std::cout << "  Entropy H(S)    : " << r.entropy    << " bits/symbol\n";
        std::cout << "  Avg code length : " << r.avgCodeLen << " bits/symbol\n";
        std::cout << std::setprecision(1);
        std::cout << "  CR              : " << r.compressionRatio << "%  ("
                  << 100.0 - r.compressionRatio << "% saved)\n";
        std::cout << std::setprecision(2);
        std::cout << "  Encode / Decode : " << r.encodeMicros << " us  /  " << r.decodeMicros << " us\n";
        std::cout << std::setprecision(1);
        std::cout << "  Throughput      : " << r.encodeThroughput << " MB/s enc  |  "
                  << r.decodeThroughput << " MB/s dec\n";
        std::cout << "  Round-trip      : " << (r.roundTripPass ? "PASS" : "FAIL") << "\n\n";
    }

    // ── TC-R: highly repetitive (1,024 bytes, 6 distinct symbols) ─────────
    results.push_back(runTest(
        "TC-R",
        "Repetitive / skewed (1 KB)",
        generateRepetitive()
    ));

    // ── TC-U: near-uniform (1,024 bytes, 256 distinct symbols) ────────────
    results.push_back(runTest(
        "TC-U",
        "Uniform / random-like (1 KB)",
        generateUniform()
    ));

    // ── Summary tables ─────────────────────────────────────────────────────
    printSummaryTable(results);

    // ── Overall pass/fail ──────────────────────────────────────────────────
    int failed = 0;
    for (auto& r : results)
        if (!r.roundTripPass) ++failed;

    rule();
    if (failed == 0)
        std::cout << "All " << results.size() << " test cases PASSED round-trip verification.\n";
    else
        std::cout << failed << " test case(s) FAILED round-trip verification.\n";
    rule();
    std::cout << "\n";

    return failed == 0 ? 0 : 1;
}
