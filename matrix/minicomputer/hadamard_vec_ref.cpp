// hadamard_vec_ref.cpp - reference for hadamard_vec.txt: H = A o B (8x8), column sums of H, S.
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra hadamard_vec_ref.cpp -o ref && ./ref hadamard_vec_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "hadamard_vec_data.byte");
    long colsum[8] = {0}, S = 0;
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            long h = w.at(8 * i + j) * w.at(64 + 8 * i + j);     // A at 0, B at 256
            std::printf("%d: %ld\n", 512 + 32 * i + 4 * j, h);    // H at 512
            colsum[j] += h;
            S += h;
        }
    for (int j = 0; j < 8; j++) std::printf("%d: %ld\n", 768 + 4 * j, colsum[j]);
    std::printf("800: %ld\n", S);
    return 0;
}
