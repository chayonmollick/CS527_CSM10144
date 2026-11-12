// Q014 reference: row N of Pascal's triangle, C(N,0), C(N,1), ..., C(N,N). The same task as
// solution.txt, solved directly in C++ (it does not run solution.txt), to get the expected
// result words for any data file.
//
//   g++ -std=c++17 reference.cpp -o reference
//   ./reference pascal_row_data.byte            prints  8: 1  12: 10  16: 45  ...  48: 1
//
// Data:   N at 0 (0 <= N <= 33: C(34,17) no longer fits in 32 bits)
// Result: C(N,k) at 8 + 4k for k = 0..N (address 4 is scratch for the program: not printed)
// Output: one line "<byte address>: <signed value>" per result word, in address order.
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// ---- The data memory, as the simulator sees it (the same code in every reference.cpp) ----
// The .byte file is a list of hex bytes ("0A 00 00 00" = 10). The simulator loads it at
// address 0 of the task's 4096-byte data space; the rest of the space is 0. A word is 4 bytes,
// little-endian (the lowest address holds the lowest byte), so word i is at byte address 4i
// and -1 is "FF FF FF FF".
const long long DATA_SPACE = 4096;                   // bytes
std::vector<int32_t> mem(DATA_SPACE / 4);            // mem[i] = the signed word at address 4i
std::map<long long, int32_t> result;                 // result words: byte address -> value

[[noreturn]] void fail(const std::string &msg) {
    std::cerr << "reference: " << msg << "\n";
    std::exit(1);
}

// Stop when the data breaks a rule of the task: the task has no defined answer then.
void need(bool ok, const char *rule) {
    if (!ok) fail(std::string("the data does not fit the task: ") + rule);
}

void load(const char *path) {
    std::ifstream in(path);
    if (!in) fail(std::string("cannot open ") + path);
    std::string tok;
    for (long long i = 0; in >> tok; i++) {          // i = the address of this byte
        char *end = nullptr;
        unsigned long byte = std::strtoul(tok.c_str(), &end, 16);
        if (*end != '\0' || byte > 0xFF) fail("'" + tok + "' is not a hex byte 00..FF");
        if (i >= DATA_SPACE) fail("the file is larger than the 4096-byte data space");
        mem[i / 4] = int32_t(uint32_t(mem[i / 4]) | uint32_t(byte) << (8 * (i % 4)));
    }
}

// Stop unless the count words from byte address addr are inside the data space.
void inside(long long addr, long long count) {
    if (addr < 0 || addr % 4 != 0 || count < 0 || addr + 4 * count > DATA_SPACE)
        fail("the words at " + std::to_string(addr) + " .. " +
             std::to_string(addr + 4 * count - 1) + " are outside the data space 0 .. 4095");
}

// count words from byte address addr: the program's [addr], [addr + 4], ...
std::vector<int32_t> words(long long addr, long long count) {
    inside(addr, count);
    return std::vector<int32_t>(mem.begin() + addr / 4, mem.begin() + addr / 4 + count);
}

int32_t word(long long addr) { return words(addr, 1)[0]; }

// A result word: the program's "[addr] = value". All of them are printed at the end.
void put(long long addr, int32_t value) {
    inside(addr, 1);
    result[addr] = value;
}

void print_results() {                               // in increasing address order
    for (const auto &[addr, value] : result) std::cout << addr << ": " << value << "\n";
}
// ---- end of the common part ----

int main(int argc, char **argv) {
    if (argc != 2) fail("usage: ./reference <data.byte>");
    load(argv[1]);
    const int32_t n = word(0);                       // N
    need(n >= 0 && n <= 33, "0 <= N <= 33");

    // Start from row 0 = 1 0 0 ... and apply Pascal's rule new[j] = old[j] + old[j-1] N times.
    // Going from right to left updates the row in place: row[j-1] still holds the old value
    // when row[j] needs it. (The program does the same 8 values at a time with vector adds,
    // block by block from right to left.)
    std::vector<int32_t> row(n + 1, 0);
    row[0] = 1;
    for (int i = 1; i <= n; i++)                     // row i from row i - 1
        for (int j = i; j >= 1; j--) row[j] += row[j - 1];
    for (int k = 0; k <= n; k++) put(8 + 4 * k, row[k]);   // C(N,k) at 8 + 4k
    print_results();
}
