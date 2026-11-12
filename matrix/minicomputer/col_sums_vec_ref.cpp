// col_sums_vec_ref.cpp - reference for col_sums_vec.txt: the 8 column sums of an R x 8 matrix.
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra col_sums_vec_ref.cpp -o ref && ./ref col_sums_vec_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "col_sums_vec_data.byte");
    long R = w.at(0);                              // R at 0, A[i][j] at 4 + 32i + 4j
    for (int j = 0; j < 8; j++) {
        long s = 0;
        for (long i = 0; i < R; i++) s += w.at(1 + 8 * i + j);
        std::printf("%ld: %ld\n", 4 + 32 * R + 4 * j, s);
    }
    return 0;
}
