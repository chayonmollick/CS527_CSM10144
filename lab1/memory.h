/* Memory (handout: Memory - "Memory is coded as a separate file"): the instruction
 * and data memories, initialize(), finalize() and the .byte file format.
 *
 * Lab 1 .byte files hold decimal bytes 0..255, four per line, as in the handout's
 * example output ("5 4 12 0"). A data word is 32 bits = four bytes, least significant
 * byte first (little-endian): 10 is "10 0 0 0", -1 is "255 255 255 255".
 */
#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <vector>
#include "config.h"

/* unsigned char, not the handout's char: a signed char would turn bytes 128..255 negative */
extern unsigned char Instruction[INSTR_SIZE];   /* handout: char Instruction[256] */
extern unsigned char Data[DATA_SIZE];           /* handout: char Data[256] */
extern int data_size;   /* bytes of Data[] in use: the data file's size, raised by a Write past it */

/* initialize(): program.byte -> Instruction[], data file -> Data[] (the rest stays 0).
 * Returns false with a message in err if a file is missing, malformed or too big. */
bool initialize(const std::string &program_path, const std::string &data_path, std::string &err);

/* finalize(): write Data[0 .. data_size-1], rounded up to whole words, to out_path. */
bool finalize(const std::string &out_path);

/* .byte file input/output: decimal bytes, four per line (the compiler writes program.byte with it). */
bool read_byte_file(const std::string &path, std::vector<unsigned char> &bytes, std::string &err);
bool write_byte_file(const std::string &path, const std::vector<unsigned char> &bytes);

#endif
