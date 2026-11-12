/* Compiler: source program (.txt) -> byte code (program.byte format).
 * Two passes: pass 1 records the instruction index of every label, pass 2
 * encodes each statement into one 4-byte instruction <opcode> <dest> <op1> <op2>.
 */
#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <vector>

struct Program {
    std::string name;                    /* program file stem, e.g. "sum_array" */
    std::vector<unsigned char> code;     /* byte code, 4 bytes per instruction, ends with HALT */
    std::vector<std::string> source;     /* every source line as written (Lab 6 task 1) */
    std::vector<int> line_of;            /* instruction index -> source line index; -1 = added HALT */
    std::vector<bool> is_label;          /* source line is a label (shown without a count in .freq) */
    bool has_source = false;             /* false when the task was started from a .byte file */
    int ro_start = 0, ro_end = -1;       /* Q072: .readonly data bytes ro_start..ro_end (none if end < start) */
};

/* Compile a source file. On error returns false with "file:line: message" in err. */
bool compile_file(const std::string &src_path, Program &prog, std::string &err);

/* Load an already compiled program.byte (no source text available). */
bool load_bytecode(const std::string &byte_path, Program &prog, std::string &err);

/* Write prog.code in program.byte format (four hex bytes per line). */
bool write_program_byte(const std::string &path, const Program &prog);

/* Human-readable form of one instruction, for listings and traces. */
std::string disassemble(int opcode, int dest, int op1, int op2);

/* "dir/sum_array.txt" -> "sum_array" */
std::string file_stem(const std::string &path);

#endif
