// max_elem_ref.cpp - reference for max_elem.txt: largest element of an R x C matrix, its (row, col).
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra max_elem_ref.cpp -o ref && ./ref max_elem_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "max_elem_data.byte");
    long R = w.at(0), C = w.at(1);                 // R at 0, C at 4, A[i][j] at 8 + 4(iC + j)
    long best = w.at(2), bi = 0, bj = 0;
    for (long i = 0; i < R; i++)
        for (long j = 0; j < C; j++)
            if (w.at(2 + i * C + j) > best) { best = w.at(2 + i * C + j); bi = i; bj = j; }  // first occurrence
    long at = 8 + 4 * R * C;
    std::printf("%ld: %ld\n%ld: %ld\n%ld: %ld\n", at, best, at + 4, bi, at + 8, bj);
    return 0;
}
