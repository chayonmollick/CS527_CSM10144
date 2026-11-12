/* Memory: the physical memory array and the .byte file format.
 * Lab 1-4 had separate Instruction[] and Data[] arrays; from Lab 5 every task's
 * instruction and data pages live in this one physical memory, reached through
 * the page table (see getPhysicalAddress in os.cpp).
 */
#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <vector>
#include "config.h"

extern unsigned char memory[MEMSIZE];   /* physical memory; frame f = bytes f*PAGESIZE .. +PAGESIZE-1 */

/* .byte files: hex bytes (no 0x), four per line, e.g. "0A 00 00 00" = 10.
 * 32-bit words are little-endian (lowest address = least significant byte),
 * so -1 is "FF FF FF FF". Returns false (and fills err) on a bad file. */
bool read_byte_file(const std::string &path, std::vector<unsigned char> &bytes, std::string &err);
bool write_byte_file(const std::string &path, const std::vector<unsigned char> &bytes);

/* Helpers for 32-bit little-endian words inside a byte vector. */
int  get_word(const std::vector<unsigned char> &bytes, int addr);
void put_word(std::vector<unsigned char> &bytes, int addr, int value);

#endif
