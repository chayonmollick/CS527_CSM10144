/* Compiler: source program (.txt) -> byte code (program.byte format).
 * Two passes: pass 1 records the instruction index of every label, pass 2
 * encodes each statement into one 4-byte instruction <opcode> <dest> <op1> <op2>.
 */
#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <vector>

struct Program {
    std::vector<unsigned char> code;     /* byte code, 4 bytes per instruction, ends with 00 00 00 00 */
    std::vector<std::string> source;     /* every source line as written (for the -c listing) */
    std::vector<int> line_of;            /* instruction index -> source line index; -1 = added end of program */
};

/* compile(): compile a source file. On error returns false with "file:line: message" in err. */
bool compile(const std::string &src_path, Program &prog, std::string &err);

/* Write prog.code in program.byte format (four hex bytes per line). */
bool write_program_byte(const std::string &path, const Program &prog);

/* Human-readable form of one instruction, for the -c listing. */
std::string disassemble(int opcode, int dest, int op1, int op2);

/* "dir/array_add_data.byte" -> "array_add_data" */
std::string file_stem(const std::string &path);

#endif
