// row_sums_ref.cpp - reference for row_sums.txt: row sums, the largest row sum and its row.
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra row_sums_ref.cpp -o ref && ./ref row_sums_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "row_sums_data.byte");
    long R = w.at(0), C = w.at(1);                 // R at 0, C at 4, A[i][j] at 8 + 4(iC + j)
    long S = 8 + 4 * R * C;                        // S right after A
    long best = 0, best_row = 0;
    for (long i = 0; i < R; i++) {
        long s = 0;
        for (long j = 0; j < C; j++) s += w.at(2 + i * C + j);
        std::printf("%ld: %ld\n", S + 4 * i, s);
        if (i == 0 || s > best) { best = s; best_row = i; }   // first row of a tie
    }
    std::printf("%ld: %ld\n", S + 4 * R, best);
    std::printf("%ld: %ld\n", S + 4 * R + 4, best_row);
    return 0;
}
