/* Compiler (handout: Language specification, Byte code specification):
 * source program (.txt) -> byte code, one 4-byte instruction per statement,
 * stored in program.byte (decimal, one instruction per line).
 */
#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <vector>

struct Program {
    std::vector<unsigned char> code;     /* byte code, 4 bytes per instruction, ends with 0 0 0 0 */
    std::vector<std::string> source;     /* every source line as written (for the -c listing) */
    std::vector<int> line_of;            /* instruction index -> source line index; -1 = the added end */
};

/* compile(): compile src_path and write its byte code to byte_path (program.byte).
 * On an error nothing is written and err holds "file:line: message". */
bool compile(const std::string &src_path, const std::string &byte_path, Program &prog, std::string &err);

/* "examples/sum4_data.byte" -> "sum4_data" (names the output file) */
std::string file_stem(const std::string &path);

#endif
