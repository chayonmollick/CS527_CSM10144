// sum_all_vec_ref.cpp - reference for sum_all_vec.txt: sum and integer mean of an R x C matrix.
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra sum_all_vec_ref.cpp -o ref && ./ref sum_all_vec_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "sum_all_vec_data.byte");
    long R = w.at(0), C = w.at(1), N = R * C, sum = 0;   // R at 0, C at 4, A at 8
    for (long k = 0; k < N; k++) sum += w.at(2 + k);
    long at = 8 + 4 * N;
    std::printf("%ld: %ld\n", at, sum);
    std::printf("%ld: %ld\n", at + 4, sum / N);          // C++ division rounds towards 0, as the machine
    return 0;
}
