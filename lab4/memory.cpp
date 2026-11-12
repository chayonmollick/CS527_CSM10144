/* Memory (handout "Memory", coded as a separate file). Lab 4: one instruction and one data
 * memory per processor, Instruction[NP][256] and Data[NP][4096]; .byte file input and output.
 */
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "memory.h"

unsigned char Instruction[NP][INSTR_SIZE];
unsigned char Data[NP][DATA_SIZE];
int data_size[NP];

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
