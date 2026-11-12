/* Memory (handout "Memory", coded as a separate file): char Instruction[256], char Data[4096],
 * initialize() / finalize(), and the .byte file format.
 */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "memory.h"

unsigned char Instruction[INSTR_SIZE];
unsigned char Data[DATA_SIZE];
int data_size = 0;

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

bool initialize(const std::string &program_byte, const std::string &data_byte, std::string &err) {
    std::vector<unsigned char> code, data;
    if (!read_byte_file(program_byte, code, err) || !read_byte_file(data_byte, data, err)) return false;
    if (code.size() % 4 != 0) { err = program_byte + ": size is not a multiple of 4 bytes"; return false; }
    if (code.size() > INSTR_SIZE) {
        err = program_byte + ": " + std::to_string(code.size()) + " bytes do not fit in Instruction[" + std::to_string(INSTR_SIZE) + "]";
        return false;
    }
    if (data.size() > DATA_SIZE) {
        err = data_byte + ": " + std::to_string(data.size()) + " bytes do not fit in Data[" + std::to_string(DATA_SIZE) + "]";
        return false;
    }
    std::memset(Instruction, 0, sizeof Instruction);    /* unused bytes read as 00 00 00 00 = end of program */
    std::memset(Data, 0, sizeof Data);
    std::copy(code.begin(), code.end(), Instruction);
    std::copy(data.begin(), data.end(), Data);
    data_size = (int)data.size();
    return true;
}

bool finalize(const std::string &out_path) {
    size_t size = ((size_t)data_size + 3) / 4 * 4;      /* whole 32-bit words */
    return write_byte_file(out_path, std::vector<unsigned char>(Data, Data + size));
}
