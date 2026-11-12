/* Memory (handout: Memory - "Memory is coded as a separate file"): the instruction
 * and data memories, initialize(), finalize() and the .byte file format.
 *
 * Lab 2 .byte files: "data.byte and program.byte are stored as an array of hex bytes.
 * In each line four bytes in hex format (without prefix 0x) separated by space".
 * A data word is 32 bits = four bytes, least significant byte first (little-endian):
 * 10 is "0A 00 00 00" and -1 is "FF FF FF FF".
 */
#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <vector>
#include "config.h"

/* unsigned char, not the handout's char: a signed char would turn bytes 80..FF negative */
extern unsigned char Instruction[INSTR_SIZE];   /* handout: char Instruction[256] */
extern unsigned char Data[DATA_SIZE];           /* handout: char Data[4096] */
extern int data_size;   /* bytes of Data[] in use: the data file's size, raised by a store past it */

/* initialize(): program.byte -> Instruction[], data file -> Data[] (the rest stays 0).
 * Returns false with a message in err if a file is missing, malformed or too big. */
bool initialize(const std::string &program_path, const std::string &data_path, std::string &err);

/* finalize(): write Data[0 .. data_size-1], rounded up to whole words, to out_path. */
bool finalize(const std::string &out_path);

/* .byte file input/output: hex bytes, four per line (the compiler writes program.byte with it). */
bool read_byte_file(const std::string &path, std::vector<unsigned char> &bytes, std::string &err);
bool write_byte_file(const std::string &path, const std::vector<unsigned char> &bytes);

#endif
