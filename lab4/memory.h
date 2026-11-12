/* Memory (handout "Memory"): Lab 4 gives every processor its own instruction and data
 * memory, so the arrays are two-dimensional: Instruction[NP][256], Data[NP][4096].
 * The OS fills them when it loads a task and writes Data[p] back when the task ends
 * (initialize() and finalize() in os.cpp).
 *
 * .byte files: hex bytes (no 0x), four per line, e.g. "0A 00 00 00" = 10.
 * 32-bit words are little-endian (lowest address = least significant byte),
 * so -1 is "FF FF FF FF".
 */
#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <vector>
#include "config.h"

extern unsigned char Instruction[NP][INSTR_SIZE];   /* program of the task on processor p */
extern unsigned char Data[NP][DATA_SIZE];           /* its data, byte addressable */
extern int data_size[NP];   /* bytes of Data[p] written back: the data file's size, grown by stores past its end */

/* Read / write a .byte file. read_byte_file returns false (and fills err) on a bad file. */
bool read_byte_file(const std::string &path, std::vector<unsigned char> &bytes, std::string &err);
bool write_byte_file(const std::string &path, const std::vector<unsigned char> &bytes);

#endif
