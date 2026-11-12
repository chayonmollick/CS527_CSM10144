/* main (handout "Main file"): compile(); initialise(); reset(); fetch/decode/execute; finalize().
 *
 *   ./sim prog.txt data.byte [out.byte]       compile prog.txt into program.byte, then run it
 *   ./sim program.byte data.byte [out.byte]   run an already compiled program
 *   ./sim -c prog.txt [out.byte]              compile only: write program.byte (or out.byte)
 *                                             and print the byte-code listing
 * The final data memory is written to out.byte, by default <data>.out in the current
 * directory (e.g. array_add_data.out); give data.byte itself to overwrite it, as in the handout.
 * Exit code: 0 = the program reached its end, 1 = compile error, bad file or run-time error.
 */
#include <cstdio>
#include <cstring>
#include <string>
#include "compiler.h"
#include "memory.h"
#include "processor.h"

static void usage() {
    std::printf("usage: ./sim prog.txt|program.byte data.byte [out.byte]\n"
                "       ./sim -c prog.txt [out.byte]     compile only and print the byte-code listing\n");
}

static bool ends_with(const std::string &s, const std::string &tail) {
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

static int compile_only(const char *src, const char *out) {
    Program prog;
    std::string err;
    if (!compile(src, prog, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    write_program_byte(out, prog);
    std::printf("%s -> %s (%zu instructions)\n", src, out, prog.code.size() / 4);
    for (size_t i = 0; i < prog.code.size() / 4; i++) {
        const unsigned char *c = &prog.code[4 * i];
        std::string text = prog.line_of[i] >= 0 ? prog.source[prog.line_of[i]] : "(end of program)";
        size_t b = text.find_first_not_of(" \t");
        text = (b == std::string::npos) ? "" : text.substr(b);
        std::printf("%3zu: %02X %02X %02X %02X   %-18s | %s\n", i, c[0], c[1], c[2], c[3],
                    disassemble(c[0], c[1], c[2], c[3]).c_str(), text.c_str());
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !std::strcmp(argv[1], "-c")) {
        if (argc < 3 || argc > 4) { usage(); return 1; }
        return compile_only(argv[2], argc == 4 ? argv[3] : "program.byte");
    }
    if (argc < 3 || argc > 4 || argv[1][0] == '-') { usage(); return 1; }
    std::string program = argv[1], data = argv[2];
    std::string out = argc == 4 ? argv[3] : file_stem(data) + ".out";
    std::string err;

    std::string byte_file = program;                    /* a .byte program is run as it is */
    if (!ends_with(program, ".byte")) {
        Program prog;                                   /* compile(): prog.txt -> program.byte */
        if (!compile(program, prog, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
        byte_file = "program.byte";
        if (!write_program_byte(byte_file, prog)) { std::fprintf(stderr, "cannot write %s\n", byte_file.c_str()); return 1; }
        std::printf("compiled %s -> %s (%zu instructions)\n", program.c_str(), byte_file.c_str(), prog.code.size() / 4);
    }
    if (!initialize(byte_file, data, err)) {            /* program.byte -> Instruction[], data.byte -> Data[] */
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    reset();
    while (!end_of_simulation) {                        /* until opcode 0 (or an error) */
        fetch();
        if (end_of_simulation) break;                   /* PC outside Instruction[] */
        decode();
        execute();
    }
    if (!finalize(out)) { std::fprintf(stderr, "cannot write %s\n", out.c_str()); return 1; }
    std::printf("%s after %ld instructions; data memory written to %s\n",
                failed ? "stopped with an error" : "finished", steps, out.c_str());
    return failed ? 1 : 0;
}
