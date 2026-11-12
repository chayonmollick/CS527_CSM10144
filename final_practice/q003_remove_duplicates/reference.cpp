// Q003 reference: remove the repeated values from a sorted array, in place. The same task as
// solution.txt, solved directly in C++ (it does not run solution.txt), to get the expected
// result words for any data file.
//
//   g++ -std=c++17 reference.cpp -o reference
//   ./reference remove_duplicates_data.byte     prints  4: -5  8: -2  ...  24: 9  and  52: 6
//
// Data:   N at 0 (N >= 0); A[0..N-1] at 4 .. 4N, sorted in ascending order
// Result: the L distinct values, in order, at 4 .. 4L; L at 4N + 4
//         (the words after the first L may keep any values: they are not printed)
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
    const int n = word(0);                           // N
    need(n >= 0, "N >= 0");
    std::vector<int32_t> a = words(4, n);            // A[0..N-1]
    for (int i = 1; i < n; i++) need(a[i - 1] <= a[i], "A is sorted in ascending order");

    // Two indices over the same array: 'length' values are kept so far (a[0] is always kept)
    // and r reads a[1..N-1]. In a sorted array equal values are next to each other, so a[r]
    // is a new value exactly when it differs from the last kept value a[length - 1].
    int length = 0;                                  // L = 0 for an empty array
    if (n > 0) {
        length = 1;
        for (int r = 1; r < n; r++)
            if (a[r] != a[length - 1]) a[length++] = a[r];   // keep it in the next free slot
    }
    for (int i = 0; i < length; i++) put(4 + 4 * i, a[i]);   // the distinct values at 4 .. 4L
    put(4 * n + 4, length);                                 // L at 4N + 4
    print_results();
}
