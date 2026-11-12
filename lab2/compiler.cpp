/* Compiler: source program (.txt) -> byte code (program.byte).
 *
 * Lab 2 language (handout: Language specification); c = constant 0..255, decimal or 0x hex:
 *   xD = xA + xB      xD = xA + c       + - * /                    arithmetic
 *   xD = c            xD = xA                                      data movement
 *   xD = [xA]         xD = [c]                                     memory read  (square brackets)
 *   [xA] = xS         [c] = xS                                     memory write
 *   Read xD, c        Write xS, c                                  legacy Lab 1 syntax, still compiled
 *   .loop                                                          label, alone on its line
 *   BEQ .loop         BGE .done         B .loop  (= BAL)           branch: B<cond> .label
 *   % comment         a trailing ';' is ignored; commas count as blanks
 */
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include "config.h"
#include "compiler.h"
#include "memory.h"

static const char *COND_NAMES[NUM_CONDITIONS] = {
    "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE", "AL"};

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

/* handout: "Any character following % will be ignored by parser"; the example's
 * "[x2] = x1;" ends with ';'. Remove both and the surrounding blanks. */
static std::string clean_line(const std::string &raw) {
    std::string s = raw;
    size_t pct = s.find('%');
    if (pct != std::string::npos) s = s.substr(0, pct);
    s = trim(s);
    while (!s.empty() && s.back() == ';') s = trim(s.substr(0, s.size() - 1));
    return s;
}

/* "x1=[x2]" -> {"x1", "=", "[", "x2", "]"}; a comma separates like a blank */
static std::vector<std::string> tokenize(const std::string &s) {
    std::string spaced;
    for (char c : s) {
        if (c && std::strchr("=+-*/[]", c)) { spaced += ' '; spaced += c; spaced += ' '; }
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

/* Decimal "200" or hex "0xC8". */
static bool parse_number(const std::string &t, long &v) {
    if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        char *end = nullptr;
        v = std::strtol(t.c_str() + 2, &end, 16);
        return *end == '\0' && t.size() <= 10 && std::isxdigit((unsigned char)t[2]);
    }
    if (!all_digits(t) || t.size() > 9) return false;
    v = std::strtol(t.c_str(), nullptr, 10);
    return true;
}

/* handout: Control operations - B{Suffix}. "beq" -> COND_EQ, "b"/"bal" -> COND_AL, else -1 */
static int branch_condition(const std::string &word) {
    std::string w = lower(word);
    if (w.empty() || w[0] != 'b') return -1;
    std::string suffix = w.substr(1);
    if (suffix.empty()) return COND_AL;
    if (suffix == "hs") return COND_CS;          /* ARM's other names for CS and CC */
    if (suffix == "lo") return COND_CC;
    for (int c = 0; c < NUM_CONDITIONS; c++)
        if (suffix == lower(COND_NAMES[c])) return c;
    return -1;
}

/* ---------------- operands ---------------- */

struct Operand {
    enum Kind { XREG, CONST, MEM_X, MEM_C } kind = CONST;   /* MEM_X = [xA], MEM_C = [c] */
    int value = 0;
};

/* "x3" -> register 3, "12" or "0x0C" -> constant 12 */
static bool parse_simple(const std::string &tok, Operand &op, std::string &msg) {
    if (tok.size() > 1 && (tok[0] == 'x' || tok[0] == 'X') && all_digits(tok.substr(1))) {
        long n = tok.size() <= 5 ? std::strtol(tok.c_str() + 1, nullptr, 10) : 99999;
        if (n >= NUM_REGS) { msg = "register " + tok + " does not exist (x0..x255)"; return false; }
        op.kind = Operand::XREG;
        op.value = (int)n;
        return true;
    }
    long v;
    if (parse_number(tok, v)) {
        if (v < 0 || v > 255) { msg = "constant " + tok + " is out of range 0..255"; return false; }
        op.kind = Operand::CONST;
        op.value = (int)v;
        return true;
    }
    msg = "'" + tok + "' is not a register (x0..x255) or a constant (0..255)";
    return false;
}

/* handout: Support of square brackets. Parses "x3", "12", "[ x3 ]" or "[ 12 ]" at t[i]; advances i. */
static bool parse_operand(const std::vector<std::string> &t, size_t &i, Operand &op, std::string &msg) {
    if (i >= t.size()) { msg = "missing operand"; return false; }
    if (t[i] == "[") {
        if (i + 2 >= t.size() || t[i + 2] != "]") { msg = "expected [x<n>] or [<constant>]"; return false; }
        Operand inner;
        if (!parse_simple(t[i + 1], inner, msg)) return false;
        op.kind = inner.kind == Operand::XREG ? Operand::MEM_X : Operand::MEM_C;
        op.value = inner.value;
        i += 3;
        return true;
    }
    if (!parse_simple(t[i], op, msg)) return false;
    i++;
    return true;
}

static void set(unsigned char ins[4], int opcode, int dest, int op1, int op2) {
    ins[0] = (unsigned char)opcode; ins[1] = (unsigned char)dest;
    ins[2] = (unsigned char)op1;    ins[3] = (unsigned char)op2;
}

/* dest = a   (data movement, memory read, memory write; operand 1 is 0) */
static bool encode_move(const Operand &dst, const Operand &a, unsigned char ins[4], std::string &msg) {
    switch (dst.kind) {
    case Operand::XREG:
        if (a.kind == Operand::CONST) { set(ins, OP_MOVC, dst.value, 0, a.value); return true; }   /* xD = c    */
        if (a.kind == Operand::XREG)  { set(ins, OP_MOV, dst.value, 0, a.value); return true; }    /* xD = xA   */
        if (a.kind == Operand::MEM_X) { set(ins, OP_LOAD, dst.value, 0, a.value); return true; }   /* xD = [xA] */
        set(ins, OP_LOADC, dst.value, 0, a.value);                                                 /* xD = [c]  */
        return true;
    case Operand::MEM_X:
    case Operand::MEM_C:
        if (a.kind != Operand::XREG) { msg = "only a register can be written to memory: [address] = x<n>"; return false; }
        set(ins, dst.kind == Operand::MEM_C ? OP_STOREC : OP_STORE, dst.value, 0, a.value);        /* [c] = xS, [xA] = xS */
        return true;
    default:
        msg = "cannot assign to a constant";
        return false;
    }
}

/* dest = a <op> b   (handout: Arithmetic operations; the second operand may be a constant) */
static bool encode_arith(const Operand &dst, const Operand &a, const std::string &op, const Operand &b,
                         unsigned char ins[4], std::string &msg) {
    size_t k = std::string("+-*/").find(op);
    if (op.size() != 1 || k == std::string::npos) { msg = "unknown operator '" + op + "' (+ - * /)"; return false; }
    if (dst.kind != Operand::XREG) { msg = "the result of an operation must go to a register"; return false; }
    if (a.kind != Operand::XREG) { msg = "the first operand must be a register"; return false; }
    if (b.kind == Operand::XREG)  { set(ins, OP_ADD + (int)k, dst.value, a.value, b.value); return true; }   /* 01..04 D A B */
    if (b.kind == Operand::CONST) { set(ins, OP_ADDI + (int)k, dst.value, a.value, b.value); return true; }  /* 09..0C D A c */
    msg = "the second operand must be a register or a constant";
    return false;
}

/* One statement (not a label) -> one instruction. index = its instruction number. */
static bool encode_statement(const std::vector<std::string> &t, int index,
                             const std::map<std::string, int> &labels,
                             unsigned char ins[4], std::string &msg) {
    std::string head = lower(t[0]);
    size_t i = 1;

    /* handout: "Read and Write should be considered legacy instruction ... compiler
     * should still be able to compile them": the constant-address forms 0D and 0E */
    if (head == "read" || head == "write") {
        Operand reg, addr;
        std::string usage = "usage: " + t[0] + " x<n>, <address 0..255>";
        if (t.size() != 3) { msg = usage; return false; }
        if (!parse_operand(t, i, reg, msg) || !parse_operand(t, i, addr, msg)) return false;
        if (reg.kind != Operand::XREG || addr.kind != Operand::CONST) { msg = usage + " (the address is a constant)"; return false; }
        if (head == "read") set(ins, OP_LOADC, reg.value, 0, addr.value);     /* = xD = [c]: 0D D 0 c */
        else                set(ins, OP_STOREC, addr.value, 0, reg.value);    /* = [c] = xS: 0E c 0 S */
        return true;
    }

    /* handout: Control operations - B{Suffix} <label>, "<opcode> <0> <0> <relative offset>" */
    int cond = branch_condition(head);
    if (cond >= 0 && (t.size() < 2 || t[1] != "=")) {
        if (t.size() != 2 || t[1][0] != '.') { msg = "usage: B<cond> .label"; return false; }
        auto it = labels.find(t[1]);
        if (it == labels.end()) { msg = "unknown label " + t[1]; return false; }
        int offset = it->second - index;        /* in instructions: next = this instruction + offset */
        if (offset < -128 || offset > 127) { msg = "branch to " + t[1] + " is too far (the offset must fit in -128..127)"; return false; }
        set(ins, OP_BRANCH + cond, 0, 0, offset & 0xFF);
        return true;
    }

    /* everything else is an assignment: dest = a  or  dest = a <op> b */
    Operand dst, a, b;
    i = 0;
    if (!parse_operand(t, i, dst, msg)) return false;
    if (i >= t.size() || t[i] != "=") { msg = "statement not recognised (expected '=')"; return false; }
    i++;
    if (!parse_operand(t, i, a, msg)) return false;
    if (i == t.size()) return encode_move(dst, a, ins, msg);
    if (i + 2 != t.size()) { msg = "expected <dest> = <operand> <op> <operand>"; return false; }
    std::string op = t[i++];
    if (!parse_operand(t, i, b, msg)) return false;
    return encode_arith(dst, a, op, b, ins, msg);
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

    /* pass 1: label -> index of the instruction that follows it
     * (handout: labels start with '.', alphanumeric, no ':', nothing else on the line) */
    std::map<std::string, int> labels;
    int count = 0;
    for (size_t ln = 0; ln < prog.source.size(); ln++) {
        std::string s = clean_line(prog.source[ln]);
        if (s.empty()) continue;
        if (s[0] == '.') {
            bool ok = s.size() > 1;
            for (size_t k = 1; k < s.size(); k++)
                if (!std::isalnum((unsigned char)s[k]) && s[k] != '_') ok = false;
            if (!ok) { err = src_path + ":" + std::to_string(ln + 1) + ": bad label '" + s + "' (a label is .name, alone on its line)"; return false; }
            if (labels.count(s)) { err = src_path + ":" + std::to_string(ln + 1) + ": label " + s + " defined twice"; return false; }
            labels[s] = count;
        } else {
            count++;
        }
    }
    if (count + 1 > MAX_INSTRUCTIONS) {
        err = src_path + ": " + std::to_string(count) + " instructions do not fit: Instruction["
              + std::to_string(INSTR_SIZE) + "] holds " + std::to_string(MAX_INSTRUCTIONS - 1) + " plus the end of program";
        return false;
    }

    /* pass 2: encode */
    int index = 0;
    for (size_t ln = 0; ln < prog.source.size(); ln++) {
        std::string s = clean_line(prog.source[ln]);
        if (s.empty() || s[0] == '.') continue;                          /* blank, comment or label */
        unsigned char ins[4] = {0, 0, 0, 0};
        std::string msg;
        if (!encode_statement(tokenize(s), index, labels, ins, msg)) {
            err = src_path + ":" + std::to_string(ln + 1) + ": " + msg + "\n    " + trim(prog.source[ln]);
            return false;
        }
        prog.code.insert(prog.code.end(), ins, ins + 4);
        prog.line_of.push_back((int)ln);
        index++;
    }
    prog.code.insert(prog.code.end(), {OP_HALT, 0, 0, 0});              /* opcode 0 ends the simulation */
    prog.line_of.push_back(-1);
    if (!write_byte_file(byte_path, prog.code)) { err = "cannot write " + byte_path; return false; }
    return true;
}
