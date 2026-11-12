// det_adj2x2_ref.cpp - reference for det_adj2x2.txt: det, adjugate, invertible flag, A * adj A.
// Reads the data file and prints every result word as "<byte address>: <value>".
//   c++ -std=c++17 -O2 -Wall -Wextra det_adj2x2_ref.cpp -o ref && ./ref det_adj2x2_data.byte
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
    std::vector<long> w = read_words(argc > 1 ? argv[1] : "det_adj2x2_data.byte");
    long A[2][2] = {{w.at(0), w.at(1)}, {w.at(2), w.at(3)}};
    long det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
    long adj[2][2] = {{A[1][1], -A[0][1]}, {-A[1][0], A[0][0]}};
    std::printf("16: %ld\n", det);
    for (int e = 0; e < 4; e++) std::printf("%d: %ld\n", 20 + 4 * e, adj[e / 2][e % 2]);
    std::printf("36: %d\n", det != 0 ? 1 : 0);
    for (int i = 0; i < 2; i++)                    // A * adj A, which must equal det * I
        for (int j = 0; j < 2; j++)
            std::printf("%d: %ld\n", 40 + 8 * i + 4 * j, A[i][0] * adj[0][j] + A[i][1] * adj[1][j]);
    return 0;
}
