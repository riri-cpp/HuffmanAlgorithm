# Huffman Coding — Lossless Data Compression in C++

> **Course:** Design and Analysis of Algorithms  
> **Project Title:** Less is More: How the Huffman Algorithm Optimizes Data Representation  
---

## Table of Contents

1. [Overview](#overview)  
2. [Repository Structure](#repository-structure)  
3. [Algorithm Summary](#algorithm-summary)  
4. [Compressed File Format](#compressed-file-format)  
5. [Build Instructions](#build-instructions)  
6. [Usage](#usage)  
7. [Test Suite](#test-suite)  
8. [Test Case Results](#test-case-results)  
9. [Complexity Reference](#complexity-reference)  
10. [References](#references)  

---

## Overview

This repository contains a complete C++17 implementation of the **Huffman coding algorithm**, a greedy, lossless data compression scheme originally devised by David A. Huffman in 1952. The implementation covers all five phases of the algorithm — frequency counting, Huffman tree construction, code generation, encoding, and decoding — and is accompanied by a full test suite that replicates the nine test cases reported in Section VIII of the project paper.

Huffman coding assigns shorter binary codes to more frequent symbols and longer codes to rarer ones, producing a **prefix-free** code tree that is provably optimal for symbol-by-symbol entropy coding. For typical English prose, the implementation achieves compression ratios of **55–60%** (saving 40–45% of the original file size) with encoding throughput of approximately **58 MB/s** on a standard x86-64 machine.

---

## Repository Structure

```
huffman-repo/
├── include/
│   └── huffman.h          # All declarations, type aliases, binary format spec
├── src/
│   ├── huffman.cpp        # Core implementation — all five algorithm phases
│   └── main.cpp           # Command-line interface (compress / decompress)
├── tests/
│   └── test_all.cpp       # Full test suite — all 9 test cases from Section VIII
└── Makefile
```

### File Descriptions

| File | Purpose |
|------|---------|
| `include/huffman.h` | Declares `HuffmanNode`, `NodeComparator`, `MinHeap`, `FreqTable`, `CodeTable`, and the HUFF binary format specification |
| `src/huffman.cpp` | Implements all five phases: `buildFrequencyTable`, `buildTree`, `buildCodeTable`, `compress`, `decompress`, plus `BitWriter`/`BitReader` helpers and diagnostic utilities |
| `src/main.cpp` | CLI wrapper; handles `compress` and `decompress` sub-commands with timing and compression ratio output |
| `tests/test_all.cpp` | Runs TC1–TC7, TC-R, and TC-U; prints frequency tables, Huffman codes, entropy metrics, and summary tables matching the paper |
| `Makefile` | Builds both the compressor binary and the test suite |

---

## Algorithm Summary

The algorithm is implemented as five sequential phases. Each phase maps directly to the theoretical description in Section V of the project paper.

### Phase 1 — Frequency Counting · O(n)
Scans the entire input once and tallies symbol occurrences in a fixed 256-slot integer array. Each update is O(1), giving O(n) total time with no hash collision risk.

### Phase 2 — Tree Construction · O(m log m)
Inserts one leaf node per distinct symbol into a **min-heap** (STL `priority_queue`), then repeatedly extracts the two minimum-frequency nodes, creates an internal parent node whose weight is their sum, and re-inserts the parent. After **m − 1** merges the heap holds a single root node. Each of the 3(m − 1) heap operations costs O(log m), giving O(m log m) total. For byte-level inputs where m ≤ 256, this is effectively **O(1)** relative to the input size.

### Phase 3 — Code Generation · O(m)
Performs a recursive depth-first traversal of the completed tree. A `'0'` is appended on every left branch and a `'1'` on every right branch. Each of the 2m − 1 nodes is visited exactly once.

### Phase 4 — Encoding · O(n)
Makes a second pass over the input. For each byte, its pre-computed code is retrieved from the code table in **O(1)** time and written bit-by-bit into a `BitWriter` buffer. The buffer flushes full bytes to the output stream. At the end, any remaining bits are zero-padded and the padding count is recorded in the header.

### Phase 5 — Decoding · O(n)
Reads the HUFF header and reconstructs the identical Huffman tree. Then traverses the compressed bitstream bit by bit: a `'0'` moves to the left child, a `'1'` to the right child. When a leaf is reached its symbol is emitted and the pointer resets to the root. Because the codes are prefix-free, no delimiter is needed and the decoder never backtracks.

---

## Compressed File Format

All compressed files produced by this implementation use the HUFF binary format:

```
Offset    Size        Field
------    ----------  --------------------------------------------------------
0         4 bytes     Magic number: 0x48 0x55 0x46 0x46  ("HUFF")
4         4 bytes     original_size — number of bytes in the original input
8         1 byte      m — number of distinct symbols  (0 encodes 256)
9         m × 5 B     Frequency table: symbol (1 B) + frequency (4 B, LE)
9 + 5m    1 byte      padding_bits — zero-bits appended to fill the last byte
10 + 5m   ...         Encoded bitstream
```

The header is self-contained: a decoder needs only the header to reconstruct the exact Huffman tree used during encoding. No external codebook or pre-trained model is required.

---

## Build Instructions

### Requirements

- **Compiler:** GCC 7.1+ or Clang 5.0+ (C++17 support required)
- **Platform:** Linux, macOS, or Windows (MinGW / MSYS2)
- **Build tool:** GNU Make (optional — direct compiler commands also work)

### Using Make

```bash
# Build the compressor binary
make

# Build and run the full test suite
make test

# Remove all build artefacts
make clean
```

### Direct Compilation

```bash
# Compressor binary
g++ -std=c++17 -O2 -I include/ src/huffman.cpp src/main.cpp -o huffman

# Test suite binary
g++ -std=c++17 -O2 -I include/ src/huffman.cpp tests/test_all.cpp -o test_all
```

### Windows (MSVC — Developer Command Prompt)

```bat
cl /std:c++17 /O2 /EHsc /I include/ src/huffman.cpp src/main.cpp /Fe:huffman.exe
```

---

## Usage

### Compress a file

```bash
./huffman compress <input_file>
```

Produces `<input_file>.huf` in the same directory.

```
$ ./huffman compress document.txt
Huffman Coding Compressor  |  Design and Analysis of Algorithms
------------------------------------------------------------
Reading  : document.txt
Original : 16384 bytes
Compressed: 9325 bytes
CR        : 56.9% (43.1% saved)
Time      : 0.290 ms  (53.9 MB/s)
Output    : document.txt.huf
```

### Decompress a file

```bash
./huffman decompress <compressed_file>.huf
```

Produces `<base>_decoded.<ext>` in the same directory.

```
$ ./huffman decompress document.txt.huf
Huffman Coding Compressor  |  Design and Analysis of Algorithms
------------------------------------------------------------
Reading  : document.txt.huf
Compressed: 9325 bytes
Restored  : 16384 bytes
Time      : 0.317 ms  (28.0 MB/s)
Output    : document_decoded.txt
```

### Notes

- The compressor requires the **full input to be available before encoding begins** (two-pass design). Streaming inputs are not supported.
- For very short inputs (< ~800 bytes), the header overhead typically causes the output to be larger than the original. This is expected behaviour — see [Test Case Results](#test-case-results).
- A single flipped bit in the compressed stream will cause the decoder to lose synchronisation. Add an external error-correction layer (e.g., CRC-32) for transmission over unreliable channels.

---

## Test Suite

The test suite in `tests/test_all.cpp` runs all nine test cases from Section VIII of the project paper and verifies that every compressed stream decompresses back to the exact original input (round-trip correctness).

```bash
make test
# or
./test_all
```

For short inputs (TC1–TC3, TC-R), the suite prints the full frequency table and Huffman code table so the codes can be inspected manually and cross-referenced with the tree construction diagram in Section V. For larger inputs (TC4–TC7, TC-U), it prints a condensed metrics summary. All cases end with a summary table matching Tables VIII-7 through VIII-9 in the paper.

### Test Case Definitions

| ID | Input Description | Size |
|----|------------------|------|
| TC1 | `"hello"` | 5 bytes |
| TC2 | `"abracadabra"` — the paper's running tree example | 11 bytes |
| TC3 | Pangram: `"the quick brown fox jumps over the lazy dog"` | 44 bytes |
| TC4 | Short paragraph (generated English-like prose) | 512 bytes |
| TC5 | Article excerpt (generated) | 2,048 bytes |
| TC6 | Long article (generated) | 16,384 bytes |
| TC7 | Full document (generated) | 1,048,576 bytes (1 MB) |
| TC-R | Highly repetitive: 900× `'a'`, 50× `'b'`, 30× `'c'`, 22× `'d'`, 12× `'e'`, 10× `'f'` | 1,024 bytes |
| TC-U | Near-uniform: all 256 byte values, each appearing 4 times | 1,024 bytes |

---

## Test Case Results

Results below were measured on Ubuntu 22.04 LTS, GCC 13.2, `-O2`, averaged over 10 runs.

### Compression Results

| TC | Input | m | Output | CR | Savings | Round-trip |
|----|-------|---|--------|----|---------|------------|
| TC1 | 5 B | 4 | 32 B | 640% | −540% (expands) | PASS |
| TC2 | 11 B | 5 | 38 B | 346% | −246% (expands) | PASS |
| TC3 | 44 B | 27 | 169 B | 393% | −293% (expands) | PASS |
| TC4 | 512 B | 32 | 443 B | 87% | **13% saved** | PASS |
| TC5 | 2,048 B | 53 | 1,401 B | 68% | **32% saved** | PASS |
| TC6 | 16,384 B | 53 | 9,325 B | 57% | **43% saved** | PASS |
| TC7 | 1,048,576 B | 53 | 579,495 B | 55% | **45% saved** | PASS |
| TC-R | 1,024 B | 6 | 201 B | 20% | **80% saved** | PASS |
| TC-U | 1,024 B | 256 | 2,314 B | 226% | −126% (expands) | PASS |

**Key observations:**
- TC1–TC3 expand because the header overhead (fixed per distinct symbol) outweighs the data savings for very short inputs. The encoded bitstream itself is shorter than the original in all cases.
- TC-R (heavily skewed distribution) achieves 80% savings, the best result in the suite, because `'a'` receives a 1-bit code and covers 88% of the input.
- TC-U (perfectly uniform distribution) does not compress at all — every symbol gets an 8-bit code identical to its original fixed-length encoding, which is the correct and expected behaviour at maximum entropy.
- From TC5 onward, results stabilise at 55–69% CR, consistent with English text entropy of approximately 4.4–4.5 bits/symbol versus 8 bits in the uncompressed representation.

### Performance Results

| TC | Input | Encode | Decode | Enc MB/s | Dec MB/s |
|----|-------|--------|--------|----------|----------|
| TC1 | 5 B | 1.1 μs | 0.5 μs | — | — |
| TC2 | 11 B | 1.2 μs | 0.5 μs | — | — |
| TC3 | 44 B | 5.3 μs | 2.1 μs | — | — |
| TC4 | 512 B | 17.2 μs | 10.6 μs | 28 MB/s | 40 MB/s |
| TC5 | 2,048 B | 43.1 μs | 35.4 μs | 45 MB/s | 38 MB/s |
| TC6 | 16,384 B | 289.8 μs | 317.2 μs | 54 MB/s | 28 MB/s |
| TC7 | 1,048,576 B | 17,230 μs | 20,570 μs | 58 MB/s | 27 MB/s |
| TC-R | 1,024 B | 12.2 μs | 7.1 μs | 80 MB/s | 27 MB/s |
| TC-U | 1,024 B | 77.5 μs | 45.5 μs | 13 MB/s | 49 MB/s |

**Key observations:**
- Tree construction time (the O(m log m) phase) is effectively constant at ~1–3 μs for typical prose inputs (m ≤ 53), confirming that it is negligible compared to the linear encoding pass.
- TC-U shows the highest tree construction overhead (~10 μs) because all 256 leaf nodes must be inserted and merged — this is the O(m log m) worst case at m = 256.
- Encoding throughput stabilises at ~58 MB/s for large inputs, consistent with published benchmarks for software Huffman encoding (~252 MB/s in optimised implementations; the gap reflects the interpreted bit-string code table used here versus a packed-integer representation).

---

## Complexity Reference

| Phase | Time | Space | Notes |
|-------|------|-------|-------|
| Frequency counting | O(n) | O(m) | One pass; O(1) per symbol with array |
| Tree construction | O(m log m) | O(m) | 3(m−1) heap ops; constant for m ≤ 256 |
| Code generation | O(m) | O(m log m) | Each node visited once; codes stored as strings |
| Encoding | O(n) | O(1) | O(1) lookup per symbol; fixed bit buffer |
| Decoding | O(n) | O(m) | O(depth) per symbol; tree only, no code table |
| **Overall** | **O(n log m)** | **O(n + m)** | Simplifies to O(n) for byte-level inputs |

The entropy bound satisfied by the implementation is:

```
H(S) ≤ L < H(S) + 1
```

where H(S) is the source entropy in bits/symbol and L is the average Huffman code length in bits/symbol. The redundancy L − H(S) measured across all test cases was below 0.5 bits/symbol in every case.

---

## References

- Huffman, D. (1952). A Method for the Construction of Minimum-Redundancy Codes. *Proceedings of the IRE*, 9, 1098–1101.
- Moffat, A. (2019). Huffman Coding. *ACM Computing Surveys*, 4, 1–35.
- Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2009). *Introduction to Algorithms* (3rd ed.). MIT Press.
- Shahbahrami, A., et al. (2011). Evaluation of Huffman and Arithmetic Algorithms for Multimedia Compression Standards. *International Journal of Computer Science, Engineering and Applications*, 4, 34–47.
- Fannos, J. T., et al. (2023). Performance evaluation of various source code techniques. *AIP Conference Proceedings*, 020018.
- Stanford University. (2023). CS106B Huffman Coding. https://web.stanford.edu/class/archive/cs/cs106b/cs106b.1242/lectures/24-huffman/
- Cover, T. M., & Thomas, J. A. (2006). *Elements of Information Theory* (2nd ed.). Wiley-Interscience.
- Salomon, D. (2007). *Data Compression: The Complete Reference*. Springer.

---

*Batangas State University — Alangilan Campus · College of Informatics and Computing Sciences*
