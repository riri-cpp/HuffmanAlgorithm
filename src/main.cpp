#include "huffman.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
// I/O helpers
// =============================================================================

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open file for reading: " + path);

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buf.data()), size))
        throw std::runtime_error("Read error: " + path);

    return buf;
}

static void writeFile(const std::string& path,
                      const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open file for writing: " + path);
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    if (!file)
        throw std::runtime_error("Write error: " + path);
}

// Strip everything after the final '.' to get the base name.
static std::string baseName(const std::string& path) {
    size_t dot = path.rfind('.');
    return (dot == std::string::npos) ? path : path.substr(0, dot);
}

// Extract the extension including the dot (e.g., ".txt").
static std::string extension(const std::string& path) {
    size_t dot = path.rfind('.');
    return (dot == std::string::npos) ? "" : path.substr(dot);
}

// =============================================================================
// Compression sub-command
// =============================================================================
static int cmdCompress(const std::string& inputPath) {
    std::cout << "Reading  : " << inputPath << "\n";
    std::vector<uint8_t> input = readFile(inputPath);

    std::cout << "Original : " << input.size() << " bytes\n";
    if (input.empty()) {
        std::cerr << "Error: input file is empty.\n";
        return 1;
    }

    // ── Run compression with timing ───────────────────────────────────────
    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> compressed = compress(input);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ── Write output ──────────────────────────────────────────────────────
    std::string outPath = inputPath + ".huf";
    writeFile(outPath, compressed);

    // ── Report ────────────────────────────────────────────────────────────
    double ratio   = 100.0 * static_cast<double>(compressed.size())
                           / static_cast<double>(input.size());
    double savings = 100.0 - ratio;
    double mbps    = (static_cast<double>(input.size()) / 1048576.0) / (ms / 1000.0);

    std::cout << "Compressed: " << compressed.size() << " bytes\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "CR        : " << ratio << "% ";
    if (savings > 0)
        std::cout << "(" << savings << "% saved)\n";
    else
        std::cout << "(expanded by " << -savings << "%)\n";
    std::cout << "Time      : " << std::setprecision(3) << ms << " ms  ("
              << std::setprecision(1) << mbps << " MB/s)\n";
    std::cout << "Output    : " << outPath << "\n";

    return 0;
}

// =============================================================================
// Decompression sub-command
// =============================================================================
static int cmdDecompress(const std::string& inputPath) {
    std::cout << "Reading  : " << inputPath << "\n";
    std::vector<uint8_t> compressed = readFile(inputPath);

    std::cout << "Compressed: " << compressed.size() << " bytes\n";

    // ── Run decompression with timing ─────────────────────────────────────
    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> output = decompress(compressed);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ── Build output path: strip ".huf", insert "_decoded" before ext ─────
    std::string noHuf   = baseName(inputPath);           // strip ".huf"
    std::string ext     = extension(noHuf);              // e.g., ".txt"
    std::string outPath = baseName(noHuf) + "_decoded" + ext;

    writeFile(outPath, output);

    double mbps = (static_cast<double>(output.size()) / 1048576.0) / (ms / 1000.0);

    std::cout << "Restored  : " << output.size() << " bytes\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Time      : " << ms << " ms  ("
              << std::setprecision(1) << mbps << " MB/s)\n";
    std::cout << "Output    : " << outPath << "\n";

    return 0;
}

// =============================================================================
// Entry point
// =============================================================================
int main(int argc, char* argv[]) {
    std::cout << "Huffman Coding Compressor  |  Design and Analysis of Algorithms\n";
    std::cout << std::string(60, '-') << "\n";

    if (argc < 3) {
        std::cerr << "Usage:\n";
        std::cerr << "  " << argv[0] << " compress   <input_file>\n";
        std::cerr << "  " << argv[0] << " decompress <compressed_file>.huf\n";
        return 1;
    }

    std::string cmd  = argv[1];
    std::string path = argv[2];

    try {
        if (cmd == "compress")
            return cmdCompress(path);
        if (cmd == "decompress")
            return cmdDecompress(path);

        std::cerr << "Unknown command '" << cmd
                  << "'.  Use 'compress' or 'decompress'.\n";
        return 1;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
