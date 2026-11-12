// rotate90_ref.cpp - reference for rotate90.txt: N x N matrix rotated 90 degrees clockwise.
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra rotate90_ref.cpp -o ref && ./ref rotate90_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "rotate90_data.byte");
    long N = w.at(0);                              // N at 0, A[i][j] at 4 + 4(iN + j)
    std::vector<long> B(N * N);
    for (long i = 0; i < N; i++)
        for (long j = 0; j < N; j++) B[j * N + (N - 1 - i)] = w.at(1 + i * N + j);   // B[j][N-1-i] = A[i][j]
    for (long k = 0; k < N * N; k++) std::printf("%ld: %ld\n", 4 + 4 * k, B[k]);
    return 0;
}
