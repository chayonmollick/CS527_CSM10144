/* main: compile and run programs on the simulated computer.
 *
 *   ./sim prog1.txt data1.byte [prog2.txt data2.byte ...]
 *         start these tasks; the shell then accepts more ("exit" or end of input stops it)
 *   ./sim -b prog.txt data.byte ...   batch: no shell, stop when every task has finished
 *   ./sim -t ...                      trace: print every executed instruction on stderr
 *   ./sim -c prog.txt [out.byte]      compile only: write out.byte (default program.byte)
 *                                     and print the byte-code listing
 * A program may also be given as an already compiled .byte file.
 * Results: <data>_<pid>.out (final data memory), <program>_<pid>.freq (Lab 6 task 1),
 *          log.txt (print output).
 */
#include <cstdio>
#include <cstring>
#include <string>
#include "compiler.h"
#include "os.h"
#include "processor.h"

static void usage() {
    std::printf("usage: ./sim [-b] [-t] prog1.txt data1.byte [@N] [prog2.txt data2.byte [@N] ...]   (@N: processor N only)\n"   /* Q093 */
                "       ./sim -c prog.txt [out.byte]\n"
                "  -b  batch mode: no shell, stop when all tasks finish\n"
                "  -t  trace every executed instruction (stderr)\n"
                "  -c  compile only and print the byte-code listing\n");
}

static int compile_only(const char *src, const char *out) {
    Program prog;
    std::string err;
    if (!compile_file(src, prog, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
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
    bool batch = false;
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (!std::strcmp(argv[i], "-b")) batch = true;
        else if (!std::strcmp(argv[i], "-t")) trace = 1;
        else if (!std::strcmp(argv[i], "-c")) {
            if (i + 1 >= argc) { usage(); return 1; }
            return compile_only(argv[i + 1], i + 2 < argc ? argv[i + 2] : "program.byte");
        } else {
            usage();
            return std::strcmp(argv[i], "-h") == 0 ? 0 : 1;
        }
    }
    int k = i;                                   /* Q093: each task is "prog data [@N]" */
    while (k + 1 < argc) k += (k + 2 < argc && argv[k + 2][0] == '@') ? 3 : 2;
    if (k != argc) { usage(); return 1; }

    os_init(batch);
    int bad = 0;
    while (i < argc) {                           /* Q093: pass the @N word on, if there is one */
        bool pinned = i + 2 < argc && argv[i + 2][0] == '@';
        if (create_task(argv[i], argv[i + 1], pinned ? argv[i + 2] : "") < 0) bad++;
        i += pinned ? 3 : 2;
    }
    scheduler();
    return (bad || failed_task_count()) ? 1 : 0;
}
