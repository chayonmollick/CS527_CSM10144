/* Memory (handout: Memory): the physical memory array and the .byte file format.
 * Labs 1-4 gave every processor its own Instruction[] and Data[] arrays. From Lab 5 the
 * instruction and data pages of every task live in this one physical memory, and the
 * processor reaches them only through the task's page table (getPhysicalAddress, os.cpp).
 */
#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <vector>
#include "config.h"

/* Handout: char memory[MEMSIZE]. Unsigned here, so bytes 0x80..0xFF do not sign-extend when
 * four of them are put together into a 32-bit word. Frame f = bytes f*PAGESIZE .. f*PAGESIZE+PAGESIZE-1. */
extern unsigned char memory[MEMSIZE];

/* .byte files: hex bytes (no 0x), four per line, e.g. "0A 00 00 00" = 10.
 * 32-bit words are little-endian (lowest address = least significant byte),
 * so -1 is "FF FF FF FF". Returns false (and fills err) on a bad file. */
bool read_byte_file(const std::string &path, std::vector<unsigned char> &bytes, std::string &err);
bool write_byte_file(const std::string &path, const std::vector<unsigned char> &bytes);

/* Helpers for 32-bit little-endian words inside a byte vector. */
int  get_word(const std::vector<unsigned char> &bytes, int addr);
void put_word(std::vector<unsigned char> &bytes, int addr, int value);

#endif
