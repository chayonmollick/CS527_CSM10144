// scalar_mul_vec_ref.cpp - reference for scalar_mul_vec.txt: B = k*A for an R x C matrix.
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra scalar_mul_vec_ref.cpp -o ref && ./ref scalar_mul_vec_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "scalar_mul_vec_data.byte");
    long R = w.at(0), C = w.at(1), k = w.at(2), N = R * C;   // R at 0, C at 4, k at 8
    long B = 12 + 4 * N;                                      // A at 12, B right after it
    for (long i = 0; i < N; i++) std::printf("%ld: %ld\n", B + 4 * i, k * w.at(3 + i));
    return 0;
}
