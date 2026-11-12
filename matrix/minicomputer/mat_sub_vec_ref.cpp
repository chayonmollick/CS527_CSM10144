// mat_sub_vec_ref.cpp - reference for mat_sub_vec.txt: C = A - B and D = 2A - 3B (4x4).
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra mat_sub_vec_ref.cpp -o ref && ./ref mat_sub_vec_data.byte
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

// The words of a .byte file: 4 hex bytes per line, 32-bit little-endian, signed.
static std::vector<long> read_words(const char *path) {
    std::ifstream in(path);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", path); std::exit(1); }
    std::vector<uint32_t> bytes;
    std::string tok;
    while (in >> tok) bytes.push_back((uint32_t)std::stoul(tok, nullptr, 16));
    std::vector<long> w(bytes.size() / 4);
    for (size_t i = 0; i < w.size(); i++)
        w[i] = (int32_t)(bytes[4 * i] | bytes[4 * i + 1] << 8 | bytes[4 * i + 2] << 16 | bytes[4 * i + 3] << 24);
    return w;
}

int main(int argc, char **argv) {
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "mat_sub_vec_data.byte");
    for (int e = 0; e < 16; e++) {                 // element e = 4i + j, row-major
        long a = w.at(e), b = w.at(16 + e);        // A at 0, B at 64
        std::printf("%d: %ld\n", 128 + 4 * e, a - b);          // C at 128
        std::printf("%d: %ld\n", 192 + 4 * e, 2 * a - 3 * b);  // D at 192
    }
    return 0;
}
