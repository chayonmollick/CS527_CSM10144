/* Compiler: source program (.txt) -> byte code (program.byte).
 *
 * Lab 1 language (handout: Language specification):
 *   xD = xA + xB     + - * /, both operands registers       arithmetic      1..4 D A B
 *   xD = 25          a constant 0..255                      data movement   7 D c 0
 *   Read xD, 12      xD = the 32-bit word at data address    memory read     5 D a 0
 *   Write xS, 16     the word at data address = xS           memory write    6 S a 0
 * Registers (the handout's "variables") are x0..x255, an address is a constant 0..255,
 * the comma after the register is optional and blank lines are skipped. Constants in
 * arithmetic, [ ], labels, branches and % comments only arrive in Lab 2: here they are
 * compile errors, reported as file:line like every other error.
 */
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include "config.h"
#include "compiler.h"
#include "memory.h"

/* ---------------- small text helpers ---------------- */

static std::string lower(std::string s) {
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string trim(const std::string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

/* "x5=x1+x2" -> {"x5", "=", "x1", "+", "x2"}; a comma separates like a blank */
static std::vector<std::string> tokenize(const std::string &s) {
    std::string spaced;
    for (char c : s) {
        if (c && std::strchr("=+-*/", c)) { spaced += ' '; spaced += c; spaced += ' '; }
        else if (c == ',') spaced += ' ';
        else spaced += c;
    }
    std::istringstream in(spaced);
    std::vector<std::string> tokens;
    std::string w;
    while (in >> w) tokens.push_back(w);
    return tokens;
}

static bool all_digits(const std::string &s) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isdigit((unsigned char)c)) return false;
    return true;
}

/* ---------------- operands ---------------- */

struct Operand {
    enum Kind { XREG, CONST } kind = CONST;     /* register x0..x255, or a constant 0..255 */
    int value = 0;
};

/* "x12" -> register 12, "200" -> constant 200 (decimal, like everything in Lab 1) */
static bool parse_operand(const std::string &tok, Operand &op, std::string &msg) {
    if (tok.size() > 1 && (tok[0] == 'x' || tok[0] == 'X') && all_digits(tok.substr(1))) {
        long n = tok.size() <= 5 ? std::strtol(tok.c_str() + 1, nullptr, 10) : 99999;
        if (n >= NUM_REGS) { msg = "register " + tok + " does not exist (x0..x255)"; return false; }
        op.kind = Operand::XREG;
        op.value = (int)n;
        return true;
    }
    if (all_digits(tok)) {
        long v = tok.size() <= 9 ? std::strtol(tok.c_str(), nullptr, 10) : 99999;
        if (v > 255) { msg = "constant " + tok + " is out of range 0..255"; return false; }
        op.kind = Operand::CONST;
        op.value = (int)v;
        return true;
    }
    msg = "'" + tok + "' is not a register (x0..x255) or a constant (0..255)";
    return false;
}

static void set(unsigned char ins[4], int opcode, int dest, int op1, int op2) {
    ins[0] = (unsigned char)opcode; ins[1] = (unsigned char)dest;
    ins[2] = (unsigned char)op1;    ins[3] = (unsigned char)op2;
}

/* One statement (a trimmed, non-blank line) -> one instruction. */
static bool encode_statement(const std::string &line, unsigned char ins[4], std::string &msg) {
    /* Lab 2 syntax: name it, rather than give a generic error */
    if (line.find('%') != std::string::npos) {
        msg = "comments (%) are not part of the Lab 1 language (they arrive in Lab 2)";
        return false;
    }
    if (line.find_first_of("[]") != std::string::npos) {
        msg = "square brackets are not part of Lab 1: use Read xD, <address> and Write xS, <address>";
        return false;
    }
    if (line[0] == '.') {
        msg = "labels are not part of Lab 1 (no control operations until Lab 2)";
        return false;
    }

    std::vector<std::string> t = tokenize(line);
    std::string head = lower(t[0]);

    /* handout: Memory operations - Read <variable> <address>, Write <variable> <address> */
    if (head == "read" || head == "write") {
        Operand reg, addr;
        std::string usage = "usage: " + t[0] + " x<n>, <address 0..255>";
        if (t.size() != 3) { msg = usage; return false; }
        if (!parse_operand(t[1], reg, msg) || !parse_operand(t[2], addr, msg)) return false;
        if (reg.kind != Operand::XREG || addr.kind != Operand::CONST) { msg = usage + " (the address is a constant)"; return false; }
        if (head == "read") set(ins, OP_READ, reg.value, addr.value, 0);      /* 5 D a 0 */
        else                set(ins, OP_WRITE, reg.value, addr.value, 0);     /* 6 S a 0 */
        return true;
    }

    /* everything else is an assignment: xD = c  or  xD = xA <op> xB */
    if (t.size() < 3 || t[1] != "=") {
        msg = "statement not recognised (Lab 1 has xD = xA <op> xB, xD = <constant>, "
              "Read xD, <address> and Write xS, <address>)";
        return false;
    }
    Operand dst, a, b;
    if (!parse_operand(t[0], dst, msg) || !parse_operand(t[2], a, msg)) return false;
    if (dst.kind != Operand::XREG) { msg = "the left side of '=' must be a register"; return false; }

    if (t.size() == 3) {                        /* handout: Data movement - <Dest Variable> = <constant value> */
        if (a.kind != Operand::CONST) {
            msg = "data movement assigns a constant (xD = 0..255); copying a register (xD = xA) is not in Lab 1";
            return false;
        }
        set(ins, OP_MOVE, dst.value, a.value, 0);                             /* 7 D c 0 */
        return true;
    }
    if (t.size() != 5) { msg = "expected xD = xA <op> xB"; return false; }
    /* handout: Dest Variable = <Operand 1 variable> <operation> <Operand 2 variable> */
    size_t k = std::string("+-*/").find(t[3]);
    if (t[3].size() != 1 || k == std::string::npos) { msg = "unknown operator '" + t[3] + "' (+ - * /)"; return false; }
    if (!parse_operand(t[4], b, msg)) return false;
    if (a.kind != Operand::XREG || b.kind != Operand::XREG) {
        msg = "both operands must be registers: xD = xA " + t[3] + " xB (constants in arithmetic arrive in Lab 2)";
        return false;
    }
    set(ins, OP_ADD + (int)k, dst.value, a.value, b.value);                   /* 1..4 D A B */
    return true;
}

/* ---------------- public functions ---------------- */

std::string file_stem(const std::string &path) {
    size_t slash = path.find_last_of('/');
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    return (dot == std::string::npos || dot == 0) ? base : base.substr(0, dot);
}

/* compile(): "Compiler generates a byte code for every operation and finally the
 * byte code is stored in file called program.byte". */
bool compile(const std::string &src_path, const std::string &byte_path, Program &prog, std::string &err) {
    std::ifstream in(src_path);
    if (!in) { err = "cannot open " + src_path; return false; }
    prog = Program();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();      /* file saved with Windows line ends */
        prog.source.push_back(line);
    }

    for (size_t ln = 0; ln < prog.source.size(); ln++) {
        std::string s = trim(prog.source[ln]);
        if (s.empty()) continue;                                         /* blank line */
        unsigned char ins[4] = {0, 0, 0, 0};
        std::string msg;
        if (!encode_statement(s, ins, msg)) {
            err = src_path + ":" + std::to_string(ln + 1) + ": " + msg + "\n    " + s;
            return false;
        }
        prog.code.insert(prog.code.end(), ins, ins + 4);
        prog.line_of.push_back((int)ln);
    }

    int count = (int)prog.line_of.size();
    if (count + 1 > MAX_INSTRUCTIONS) {
        err = src_path + ": " + std::to_string(count) + " instructions do not fit: Instruction["
              + std::to_string(INSTR_SIZE) + "] holds " + std::to_string(MAX_INSTRUCTIONS - 1) + " plus the end of program";
        return false;
    }
    prog.code.insert(prog.code.end(), {OP_HALT, 0, 0, 0});              /* opcode 0 ends the simulation */
    prog.line_of.push_back(-1);
    if (!write_byte_file(byte_path, prog.code)) { err = "cannot write " + byte_path; return false; }
    return true;
}
