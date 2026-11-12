/* Memory (handout: Memory): Instruction[] and Data[], initialize() and finalize(). */
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include "memory.h"

unsigned char Instruction[INSTR_SIZE];
unsigned char Data[DATA_SIZE];
int data_size = 0;

/* One decimal byte, "0" .. "255". */
static bool parse_byte(const std::string &tok, unsigned char &b) {
    if (tok.empty() || tok.size() > 3) return false;
    int v = 0;
    for (char c : tok) {
        if (!std::isdigit((unsigned char)c)) return false;
        v = v * 10 + (c - '0');
    }
    if (v > 255) return false;
    b = (unsigned char)v;
    return true;
}

bool read_byte_file(const std::string &path, std::vector<unsigned char> &bytes, std::string &err) {
    std::ifstream in(path);
    if (!in) { err = "cannot open " + path; return false; }
    bytes.clear();
    std::string line, tok;
    for (int ln = 1; std::getline(in, line); ln++) {
        std::istringstream words(line);
        while (words >> tok) {
            unsigned char b;
            if (!parse_byte(tok, b)) {
                err = path + ":" + std::to_string(ln) + ": '" + tok + "' is not a decimal byte (0..255)";
                return false;
            }
            bytes.push_back(b);
        }
    }
    return true;
}

bool write_byte_file(const std::string &path, const std::vector<unsigned char> &bytes) {
    FILE *f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    for (size_t i = 0; i < bytes.size(); i++)
        std::fprintf(f, "%d%s", bytes[i], (i % 4 == 3 || i + 1 == bytes.size()) ? "\n" : " ");
    return std::fclose(f) == 0;
}

/* initialize(): "read the data from program.byte and populate Instruction memory;
 * read the data from data.byte and populate data memory". */
bool initialize(const std::string &program_path, const std::string &data_path, std::string &err) {
    std::vector<unsigned char> code, data;
    if (!read_byte_file(program_path, code, err)) return false;
    if (code.size() % 4 != 0) {
        err = program_path + ": " + std::to_string(code.size()) + " bytes is not a whole number of 4-byte instructions";
        return false;
    }
    if (code.size() > (size_t)INSTR_SIZE) {
        err = program_path + ": " + std::to_string(code.size() / 4) + " instructions do not fit in Instruction["
              + std::to_string(INSTR_SIZE) + "]";
        return false;
    }
    if (!read_byte_file(data_path, data, err)) return false;
    if (data.size() > (size_t)DATA_SIZE) {
        err = data_path + ": " + std::to_string(data.size()) + " bytes do not fit in Data[" + std::to_string(DATA_SIZE) + "]";
        return false;
    }
    std::fill(Instruction, Instruction + INSTR_SIZE, 0);
    std::fill(Data, Data + DATA_SIZE, 0);
    std::copy(code.begin(), code.end(), Instruction);
    std::copy(data.begin(), data.end(), Data);
    data_size = (int)data.size();
    return true;
}

/* finalize(): "Write data.byte" - the data memory goes out in the same format: every
 * byte the data file had, plus any word the program wrote beyond it. */
bool finalize(const std::string &out_path) {
    size_t n = ((size_t)data_size + 3) / 4 * 4;
    return write_byte_file(out_path, std::vector<unsigned char>(Data, Data + n));
}
