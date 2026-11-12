// count_zeros_ref.cpp - reference for count_zeros.txt: zeros, nonzeros and all-zero rows.
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra count_zeros_ref.cpp -o ref && ./ref count_zeros_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "count_zeros_data.byte");
    long R = w.at(0), C = w.at(1);                 // R at 0, C at 4, A[i][j] at 8 + 4(iC + j)
    long zeros = 0, zero_rows = 0;
    for (long i = 0; i < R; i++) {
        long in_row = 0;
        for (long j = 0; j < C; j++) in_row += (w.at(2 + i * C + j) == 0);
        zeros += in_row;
        zero_rows += (in_row == C);
    }
    long at = 8 + 4 * R * C;
    std::printf("%ld: %ld\n%ld: %ld\n%ld: %ld\n", at, zeros, at + 4, R * C - zeros, at + 8, zero_rows);
    return 0;
}
