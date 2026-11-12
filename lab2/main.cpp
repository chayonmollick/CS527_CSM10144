/* main (handout: Main file): "compilation and computer system should be invoked by a
 * single main file"; "It is suggested that the program file is passed as argument to
 * your final executable - using argc and argv".
 *
 *   ./sim prog.txt data.byte [out.byte]      compile prog.txt into program.byte, run it on
 *                                            data.byte, write the final data memory to
 *                                            out.byte (default: <data file name>.out)
 *   ./sim program.byte data.byte [out.byte]  run an already compiled program
 *   ./sim -c prog.txt [out.byte]             compile only: write out.byte (default
 *                                            program.byte) and print the byte-code listing
 * Exit code 0 = finished; 1 = compile error, bad file or run-time error.
 */
#include <cstdio>
#include <cstring>
#include <string>
#include "compiler.h"
#include "memory.h"
#include "processor.h"

static void usage() {
    std::printf("usage: ./sim prog.txt data.byte [out.byte]      compile, run, write the data memory to out.byte\n"
                "                                                (default <data>.out)\n"
                "       ./sim program.byte data.byte [out.byte]  run an already compiled program\n"
                "       ./sim -c prog.txt [out.byte]             compile only (default program.byte), print the listing\n");
}

static bool ends_with(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/* -c: compile, then print every instruction next to the source line it came from. */
static int compile_only(const char *src, const char *out) {
    Program prog;
    std::string err;
    if (!compile(src, out, prog, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    std::printf("%s -> %s (%zu instructions)\n", src, out, prog.code.size() / 4);
    for (size_t i = 0; i < prog.code.size() / 4; i++) {
        const unsigned char *c = &prog.code[4 * i];
        std::string text = prog.line_of[i] >= 0 ? prog.source[prog.line_of[i]] : "(end of program)";
        size_t b = text.find_first_not_of(" \t");
        text = (b == std::string::npos) ? "" : text.substr(b);
        std::printf("%3zu: %02X %02X %02X %02X   | %s\n", i, c[0], c[1], c[2], c[3], text.c_str());
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 3 && argc <= 4 && std::strcmp(argv[1], "-c") == 0)
        return compile_only(argv[2], argc == 4 ? argv[3] : "program.byte");
    if (argc < 3 || argc > 4 || argv[1][0] == '-') {
        usage();
        return (argc == 2 && std::strcmp(argv[1], "-h") == 0) ? 0 : 1;
    }
    std::string program = argv[1], data = argv[2];
    std::string out = (argc == 4) ? argv[3] : file_stem(data) + ".out";
    std::string byte_file = program, err;

    if (!ends_with(program, ".byte")) {                  /* compile(): source -> program.byte */
        Program prog;
        byte_file = "program.byte";
        if (!compile(program, byte_file, prog, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
        std::printf("compiled %s -> %s (%zu instructions)\n", program.c_str(), byte_file.c_str(), prog.code.size() / 4);
    }
    if (!initialize(byte_file, data, err)) {             /* initialize(): program.byte and data -> memory */
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    reset();
    while (!end_of_simulation) {                         /* the handout's while(end_of_simulation) misses the ! */
        fetch();
        decode();
        execute();
    }
    if (!finalize(out)) {                                /* finalize(): data memory -> out */
        std::fprintf(stderr, "cannot write %s\n", out.c_str());
        return 1;
    }
    std::printf("%s after %ld instructions; data memory written to %s\n",
                failed ? "stopped by an error" : "finished", steps, out.c_str());
    return failed ? 1 : 0;
}
