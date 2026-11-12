/* Memory (handout "Memory"): instruction and data memory, initialize() and finalize(),
 * and the .byte file format.
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

extern unsigned char Instruction[INSTR_SIZE];   /* program: 4 bytes per instruction */
extern unsigned char Data[DATA_SIZE];           /* data: byte addressable */
extern int data_size;   /* bytes finalize() writes back: the data file's size, grown by stores past its end */

/* initialize(): read program.byte into Instruction[] and data.byte into Data[] (the rest is 0). */
bool initialize(const std::string &program_byte, const std::string &data_byte, std::string &err);

/* finalize(): write Data[0 .. data_size) to out_path in the same format as data.byte. */
bool finalize(const std::string &out_path);

/* Read / write a .byte file. read_byte_file returns false (and fills err) on a bad file. */
bool read_byte_file(const std::string &path, std::vector<unsigned char> &bytes, std::string &err);
bool write_byte_file(const std::string &path, const std::vector<unsigned char> &bytes);

#endif
