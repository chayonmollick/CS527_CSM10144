/* Memory (handout: Memory): physical memory and .byte file input/output. */
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "memory.h"

unsigned char memory[MEMSIZE];

bool read_byte_file(const std::string &path, std::vector<unsigned char> &bytes, std::string &err) {
    std::ifstream in(path);
    if (!in) { err = "cannot open " + path; return false; }
    bytes.clear();
    std::string tok;
    while (in >> tok) {
        char *end = nullptr;
        unsigned long v = std::strtoul(tok.c_str(), &end, 16);
        if (*end != '\0' || v > 0xFF) { err = path + ": '" + tok + "' is not a hex byte (00..FF)"; return false; }
        bytes.push_back((unsigned char)v);
    }
    return true;
}

bool write_byte_file(const std::string &path, const std::vector<unsigned char> &bytes) {
    FILE *f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    for (size_t i = 0; i < bytes.size(); i++)
        std::fprintf(f, "%02X%s", bytes[i], (i % 4 == 3 || i + 1 == bytes.size()) ? "\n" : " ");
    std::fclose(f);
    return true;
}

int get_word(const std::vector<unsigned char> &bytes, int addr) {
    unsigned int v = 0;
    for (int k = 3; k >= 0; k--) {
        size_t i = (size_t)addr + k;
        v = (v << 8) | (i < bytes.size() ? bytes[i] : 0);
    }
    return (int)v;
}

void put_word(std::vector<unsigned char> &bytes, int addr, int value) {
    if (bytes.size() < (size_t)addr + 4) bytes.resize((size_t)addr + 4, 0);
    unsigned int v = (unsigned int)value;
    for (int k = 0; k < 4; k++) bytes[(size_t)addr + k] = (unsigned char)(v >> (8 * k));
}
