// Q004 reference: rotate an array left by k positions, in place (three reversals). The same
// task as solution.txt, solved directly in C++ (it does not run solution.txt), to get the
// expected result words for any data file.
//
//   g++ -std=c++17 reference.cpp -o reference
//   ./reference rotate_left_data.byte           prints  8: 13  12: 0  ...  40: 8
//
// Data:   N at 0 (N >= 0); k at 4 (k >= 0, may be larger than N); A[0..N-1] at 8 .. 4N + 4
// Result: A rotated left by k (by k mod N), in the same place (8 .. 4N + 4)
// Output: one line "<byte address>: <signed value>" per result word, in address order.
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
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

// Reverse a[lo..hi] in place: swap the two ends and move both towards the middle
// (the program's .rev loop, with lo and hi as addresses).
void reverse_range(std::vector<int32_t> &a, int lo, int hi) {
    for (; lo < hi; lo++, hi--) std::swap(a[lo], a[hi]);
}

int main(int argc, char **argv) {
    if (argc != 2) fail("usage: ./reference <data.byte>");
    load(argv[1]);
    const int n = word(0);                           // N
    int k = word(4);                                 // k
    need(n >= 0, "N >= 0");
    need(k >= 0, "k >= 0");
    std::vector<int32_t> a = words(8, n);            // A[0..N-1]

    // A = X Y with X = the first k elements; the rotation is Y X = (X^r Y^r)^r, where ^r is a
    // reversal: reverse a[0..k-1], reverse a[k..N-1], then reverse the whole array.
    if (n > 1) {                                     // with 0 or 1 elements nothing moves
        k = k % n;                                   // k mod N (= k - (k / N) * N)
        reverse_range(a, 0, k - 1);                  // (empty when k = 0)
        reverse_range(a, k, n - 1);
        reverse_range(a, 0, n - 1);
    }
    for (int i = 0; i < n; i++) put(8 + 4 * i, a[i]);    // the rotated array at 8 .. 4N + 4
    print_results();
}
