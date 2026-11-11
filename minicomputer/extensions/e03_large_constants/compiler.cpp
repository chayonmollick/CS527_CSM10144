/* Compiler: source program (.txt) -> byte code.
 *
 * Language (Labs 1-4):
 *   xD = xA + xB      xD = xA - 7      (+ - * /, constants 0..255)   Lab 1-2
 *   xD = 5            xD = xA                                         data movement
 *   xD = [xA]         xD = [16]        [xA] = xS       [16] = xS      memory (Lab 2)
 *   Read xD 16        Write xS 16                                     legacy (Lab 1)
 *   .loop             BEQ .loop        BAL .loop  (B<cond> .label)    labels/branches (Lab 2)
 *   vD = vA + vB      vD = vA * 3      vD = vA * xB  (+ - *)          vectors (Lab 3)
 *   vD = [xA]         vD = [32]        [xA] = vS       [32] = vS      vector memory
 *   print xR                                                          Lab 4
 *   xD = 100000       xD = -5          xD = 0x12345678                EXT e03: any 32-bit constant
 *                     (one line -> MOVC + up to 3 SHLOR instructions; other operands stay 0..255)
 *   % comment         trailing ';' and ',' separators are ignored
 */
#include <cctype>
#include <cstdint>      /* EXT e03: uint32_t */
#include <cstdio>
#include <cstdlib>      /* EXT e03: strtoull */
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

/* Remove the comment (% ...), trailing ';' and surrounding blanks. */
static std::string clean_line(const std::string &raw) {
    std::string s = raw;
    size_t pct = s.find('%');
    if (pct != std::string::npos) s = s.substr(0, pct);
    s = trim(s);
    while (!s.empty() && s.back() == ';') s = trim(s.substr(0, s.size() - 1));
    return s;
}

/* "x1=[x2]+3" -> {"x1", "=", "[", "x2", "]", "+", "3"} */
static std::vector<std::string> tokenize(const std::string &s) {
    std::string spaced;
    for (char c : s) {
        if (std::strchr("=+-*/[]", c)) { spaced += ' '; spaced += c; spaced += ' '; }
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
        return *end == '\0' && t.size() <= 10;
    }
    if (!all_digits(t) || t.size() > 9) return false;
    v = std::strtol(t.c_str(), nullptr, 10);
    return true;
}

/* "beq" -> COND_EQ, "b"/"bal" -> COND_AL, anything else -> -1 */
static int branch_condition(const std::string &word) {
    std::string w = lower(word);
    if (w.empty() || w[0] != 'b') return -1;
    std::string suffix = w.substr(1);
    if (suffix.empty()) return COND_AL;
    if (suffix == "hs") return COND_CS;          /* common aliases */
    if (suffix == "lo") return COND_CC;
    for (int c = 0; c < NUM_CONDITIONS; c++)
        if (suffix == lower(COND_NAMES[c])) return c;
    return -1;
}

/* ---------------- operands ---------------- */

struct Operand {
    enum Kind { XREG, VREG, CONST, MEM_X, MEM_C } kind = CONST;   /* MEM_X = [xA], MEM_C = [c] */
    int value = 0;
};

static bool parse_simple(const std::string &tok, Operand &op, std::string &msg) {
    char c0 = (char)std::tolower((unsigned char)(tok.empty() ? ' ' : tok[0]));
    if ((c0 == 'x' || c0 == 'v') && all_digits(tok.substr(1))) {
        bool vec = (c0 == 'v');
        int limit = vec ? NUM_VREGS : NUM_XREGS;
        long n = tok.size() <= 5 ? std::strtol(tok.c_str() + 1, nullptr, 10) : 99999;
        if (n >= limit) {
            msg = "register " + tok + " does not exist (" + (vec ? "v0..v31" : "x0..x255") + ")";
            return false;
        }
        op.kind = vec ? Operand::VREG : Operand::XREG;
        op.value = (int)n;
        return true;
    }
    long v;
    if (parse_number(tok, v)) {
        if (v < 0 || v > 255) { msg = "constant " + tok + " is out of range 0..255 (only 'xD = <constant>' takes any 32-bit value)"; return false; }   /* EXT e03: hint */
        op.kind = Operand::CONST;
        op.value = (int)v;
        return true;
    }
    msg = "'" + tok + "' is not a register (x0..x255, v0..v31) or a constant (0..255)";
    return false;
}

/* Parses "x3", "v1", "12" or "[ x3 ]" / "[ 12 ]" starting at t[i]; advances i. */
static bool parse_operand(const std::vector<std::string> &t, size_t &i, Operand &op, std::string &msg) {
    if (i >= t.size()) { msg = "missing operand"; return false; }
    if (t[i] == "[") {
        if (i + 2 >= t.size() || t[i + 2] != "]") { msg = "expected [x<n>] or [<constant>]"; return false; }
        Operand inner;
        if (!parse_simple(t[i + 1], inner, msg)) return false;
        if (inner.kind == Operand::XREG) op.kind = Operand::MEM_X;
        else if (inner.kind == Operand::CONST) op.kind = Operand::MEM_C;
        else { msg = "an address must be an x register or a constant"; return false; }
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

/* dest = a   (moves, loads and stores) */
static bool encode_move(const Operand &dst, const Operand &a, unsigned char ins[4], std::string &msg) {
    switch (dst.kind) {
    case Operand::XREG:
        if (a.kind == Operand::CONST) { set(ins, OP_MOVC, dst.value, 0, a.value); return true; }
        if (a.kind == Operand::XREG)  { set(ins, OP_MOV, dst.value, 0, a.value); return true; }
        if (a.kind == Operand::MEM_X) { set(ins, OP_LOAD, dst.value, 0, a.value); return true; }
        if (a.kind == Operand::MEM_C) { set(ins, OP_LOADC, dst.value, 0, a.value); return true; }
        msg = "an x register cannot be assigned a vector";
        return false;
    case Operand::VREG:
        if (a.kind == Operand::MEM_X) { set(ins, OP_VLOAD, dst.value, 0, a.value); return true; }
        if (a.kind == Operand::MEM_C) { set(ins, OP_VLOADC, dst.value, 0, a.value); return true; }
        if (a.kind == Operand::VREG)  { set(ins, OP_VADDI, dst.value, a.value, 0); return true; }  /* copy */
        msg = "a vector register can only be loaded from memory or copied from another vector";
        return false;
    case Operand::MEM_X:
    case Operand::MEM_C: {
        bool cst = (dst.kind == Operand::MEM_C);
        if (a.kind == Operand::XREG) { set(ins, cst ? OP_STOREC : OP_STORE, dst.value, 0, a.value); return true; }
        if (a.kind == Operand::VREG) { set(ins, cst ? OP_VSTOREC : OP_VSTORE, dst.value, 0, a.value); return true; }
        msg = "only a register can be stored to memory: [address] = x<n> or v<n>";
        return false;
    }
    default:
        msg = "cannot assign to a constant";
        return false;
    }
}

/* dest = a <op> b */
static bool encode_arith(const Operand &dst, const Operand &a, const std::string &op, const Operand &b,
                         unsigned char ins[4], std::string &msg) {
    size_t k = std::string("+-*/").find(op);
    if (op.size() != 1 || k == std::string::npos) { msg = "unknown operator '" + op + "'"; return false; }
    if (dst.kind == Operand::XREG) {
        if (a.kind != Operand::XREG) { msg = "the first operand must be an x register"; return false; }
        if (b.kind == Operand::XREG)  { set(ins, OP_ADD + (int)k, dst.value, a.value, b.value); return true; }
        if (b.kind == Operand::CONST) { set(ins, OP_ADDI + (int)k, dst.value, a.value, b.value); return true; }
        msg = "the second operand must be an x register or a constant";
        return false;
    }
    if (dst.kind == Operand::VREG) {
        if (a.kind != Operand::VREG) { msg = "the first operand of a vector operation must be a vector"; return false; }
        if (op == "/") { msg = "vector division is not supported (+ - * only)"; return false; }
        if (b.kind == Operand::VREG)  { set(ins, OP_VADD + (int)k, dst.value, a.value, b.value); return true; }
        if (b.kind == Operand::CONST) { set(ins, OP_VADDI + (int)k, dst.value, a.value, b.value); return true; }
        if (b.kind == Operand::XREG)  { set(ins, OP_VADDX + (int)k, dst.value, a.value, b.value); return true; }
        msg = "the second operand must be a vector, an x register or a constant";
        return false;
    }
    msg = "the result of an operation must go to a register";
    return false;
}

/* One statement (not a label) -> one instruction. index = its instruction number. */
static bool encode_statement(const std::vector<std::string> &t, int index,
                             const std::map<std::string, int> &labels,
                             unsigned char ins[4], std::string &msg) {
    set(ins, 0, 0, 0, 0);
    std::string head = lower(t[0]);
    size_t i = 1;

    if (head == "read" || head == "write") {                    /* legacy Lab 1 syntax */
        Operand reg, addr;
        if (t.size() != 3 || !parse_operand(t, i, reg, msg) || !parse_operand(t, i, addr, msg)
            || reg.kind != Operand::XREG || addr.kind != Operand::CONST) {
            msg = "usage: " + t[0] + " x<n> <constant address>";
            return false;
        }
        if (head == "read") set(ins, OP_LOADC, reg.value, 0, addr.value);
        else                set(ins, OP_STOREC, addr.value, 0, reg.value);
        return true;
    }
    if (head == "print") {                                      /* Lab 4 */
        Operand reg;
        if (t.size() != 2 || !parse_operand(t, i, reg, msg) || reg.kind != Operand::XREG) {
            msg = "usage: print x<n>";
            return false;
        }
        set(ins, OP_PRINT, 0, 0, reg.value);
        return true;
    }
    int cond = branch_condition(head);
    if (cond >= 0 && t.size() >= 2 && t[1] != "=") {           /* Lab 2 branches */
        if (t.size() != 2 || t[1][0] != '.') { msg = "usage: B<cond> .label"; return false; }
        auto it = labels.find(t[1]);
        if (it == labels.end()) { msg = "unknown label " + t[1]; return false; }
        int offset = it->second - index;                        /* in instructions, relative to this one */
        if (offset < -128 || offset > 127) { msg = "branch to " + t[1] + " is too far (offset must fit in -128..127)"; return false; }
        set(ins, OP_BRANCH + cond, 0, 0, offset & 0xFF);
        return true;
    }

    Operand dst, a, b;                                          /* assignment: dest = a [op b] */
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

/* ---------------- EXT e03: constants that do not fit in one byte ---------------- */

/* Decimal "100000" or hex "0x186A0", without a sign; too many digits gives a huge value
 * (strtoull saturates), which the caller rejects as out of range. */
static bool parse_wide_number(const std::string &t, unsigned long long &v) {
    bool hex = t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X');
    const char *digits = t.c_str() + (hex ? 2 : 0);
    if (!std::isxdigit((unsigned char)digits[0])) return false;   /* no sign or blank allowed here */
    char *end = nullptr;
    v = std::strtoull(digits, &end, hex ? 16 : 10);
    return *end == '\0';
}

/* Is this statement "xD = <constant>" with a constant outside 0..255 (e.g. 100000, -5, 0x12345678)?
 * Returns 1 and sets reg/value if so, 0 if the statement is anything else (encode_statement
 * handles it, so small constants still give the old single MOVC), -1 on an error (msg). */
static int wide_constant_move(const std::vector<std::string> &t, int &reg, uint32_t &value, std::string &msg) {
    bool negative = t.size() == 4 && t[2] == "-";               /* the tokenizer splits "-5" into "-" "5" */
    if ((t.size() != 3 && !negative) || t[1] != "=") return 0;
    Operand dst;
    std::string ignored;
    if (!parse_simple(t[0], dst, ignored) || dst.kind != Operand::XREG) return 0;
    unsigned long long v = 0;
    if (!parse_wide_number(t.back(), v)) return 0;
    if (!negative && v <= 255) return 0;
    if (v > (negative ? 2147483648ULL : 4294967295ULL)) {
        msg = "constant " + std::string(negative ? "-" : "") + t.back() + " does not fit in 32 bits (-2147483648 .. 4294967295)";
        return -1;
    }
    reg = dst.value;
    value = negative ? 0u - (uint32_t)v : (uint32_t)v;          /* two's complement: -5 = 0xFFFFFFFB */
    return 1;
}

/* The instructions that load value into xreg: MOVC with the highest non-zero byte, then one
 * SHLOR "x = (x << 8) | byte" for each lower byte. Returns how many (1..4) it wrote to code.
 *   100000 = 0x000186A0  ->  MOVC 0x01, SHLOR 0x86, SHLOR 0xA0                     (3)
 *   -5     = 0xFFFFFFFB  ->  MOVC 0xFF, SHLOR 0xFF, SHLOR 0xFF, SHLOR 0xFB         (4) */
static int wide_constant_code(int reg, uint32_t value, unsigned char code[4][4]) {
    int top = 3;
    while (top > 0 && ((value >> (8 * top)) & 0xFF) == 0) top--;   /* skip leading zero bytes */
    int n = 0;
    set(code[n++], OP_MOVC, reg, 0, (value >> (8 * top)) & 0xFF);
    for (int k = top - 1; k >= 0; k--)
        set(code[n++], OP_SHLOR, reg, 0, (value >> (8 * k)) & 0xFF);
    return n;
}

/* Pass 1 needs the size of every line: labels are instruction numbers. */
static int instructions_for(const std::vector<std::string> &t) {
    int reg = 0;
    uint32_t value = 0;
    std::string msg;
    unsigned char code[4][4];
    if (wide_constant_move(t, reg, value, msg) == 1) return wide_constant_code(reg, value, code);
    return 1;                                                    /* everything else (errors show in pass 2) */
}

/* ---------------- public functions ---------------- */

std::string file_stem(const std::string &path) {
    size_t slash = path.find_last_of('/');
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    return (dot == std::string::npos || dot == 0) ? base : base.substr(0, dot);
}

bool compile_file(const std::string &src_path, Program &prog, std::string &err) {
    std::ifstream in(src_path);
    if (!in) { err = "cannot open " + src_path; return false; }
    prog = Program();
    prog.name = file_stem(src_path);
    prog.has_source = true;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        prog.source.push_back(line);
    }
    prog.is_label.assign(prog.source.size(), false);

    /* pass 1: label -> instruction index */
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
            prog.is_label[ln] = true;
        } else {
            count += instructions_for(tokenize(s));      /* EXT e03: a wide constant takes up to 4 */
        }
    }
    if (count + 1 > MAX_INSTRUCTIONS) {
        err = src_path + ": program has " + std::to_string(count) + " instructions; the limit is " + std::to_string(MAX_INSTRUCTIONS - 1);
        return false;
    }

    /* pass 2: encode */
    int index = 0;
    for (size_t ln = 0; ln < prog.source.size(); ln++) {
        std::string s = clean_line(prog.source[ln]);
        if (s.empty() || s[0] == '.') continue;
        std::vector<std::string> t = tokenize(s);
        unsigned char code[4][4];                        /* EXT e03: one line -> up to 4 instructions */
        int n = 1, reg = 0;
        uint32_t value = 0;
        std::string msg;
        int wide = wide_constant_move(t, reg, value, msg);
        if (wide == 1) n = wide_constant_code(reg, value, code);
        else if (wide == 0 && !encode_statement(t, index, labels, code[0], msg)) wide = -1;
        if (wide < 0) {
            err = src_path + ":" + std::to_string(ln + 1) + ": " + msg + "\n    " + trim(prog.source[ln]);
            return false;
        }
        for (int k = 0; k < n; k++) {                    /* EXT e03: every one maps to source line ln */
            prog.code.insert(prog.code.end(), code[k], code[k] + 4);
            prog.line_of.push_back((int)ln);
            index++;
        }
    }
    prog.code.insert(prog.code.end(), {OP_HALT, 0, 0, 0});   /* end of program */
    prog.line_of.push_back(-1);
    return true;
}

bool load_bytecode(const std::string &byte_path, Program &prog, std::string &err) {
    prog = Program();
    prog.name = file_stem(byte_path);
    if (!read_byte_file(byte_path, prog.code, err)) return false;
    if (prog.code.size() % 4 != 0) { err = byte_path + ": size is not a multiple of 4 bytes"; return false; }
    if (prog.code.empty() || prog.code[prog.code.size() - 4] != OP_HALT)
        prog.code.insert(prog.code.end(), {OP_HALT, 0, 0, 0});
    if (prog.code.size() > (size_t)INSTR_SPACE) { err = byte_path + ": program is larger than " + std::to_string(INSTR_SPACE) + " bytes"; return false; }
    prog.line_of.assign(prog.code.size() / 4, -1);
    return true;
}

bool write_program_byte(const std::string &path, const Program &prog) {
    return write_byte_file(path, prog.code);
}

std::string disassemble(int op, int d, int a, int b) {
    char buf[64];
    static const char OPS[] = "+-*/";
    if (op == OP_HALT) return "halt";
    if (op >= OP_ADD && op <= OP_DIV)   { std::snprintf(buf, sizeof buf, "x%d = x%d %c x%d", d, a, OPS[op - OP_ADD], b); return buf; }
    if (op >= OP_ADDI && op <= OP_DIVI) { std::snprintf(buf, sizeof buf, "x%d = x%d %c %d", d, a, OPS[op - OP_ADDI], b); return buf; }
    if (op >= OP_VADD && op <= OP_VMUL) { std::snprintf(buf, sizeof buf, "v%d = v%d %c v%d", d, a, OPS[op - OP_VADD], b); return buf; }
    if (op >= OP_VADDI && op <= OP_VMULI) { std::snprintf(buf, sizeof buf, "v%d = v%d %c %d", d, a, OPS[op - OP_VADDI], b); return buf; }
    if (op >= OP_VADDX && op <= OP_VMULX) { std::snprintf(buf, sizeof buf, "v%d = v%d %c x%d", d, a, OPS[op - OP_VADDX], b); return buf; }
    if (op >= OP_BRANCH && op < OP_BRANCH + NUM_CONDITIONS) {
        std::snprintf(buf, sizeof buf, "B%s %+d", COND_NAMES[op - OP_BRANCH], (int)(signed char)b);
        return buf;
    }
    switch (op) {
    case OP_LOAD:    std::snprintf(buf, sizeof buf, "x%d = [x%d]", d, b); break;
    case OP_LOADC:   std::snprintf(buf, sizeof buf, "x%d = [%d]", d, b); break;
    case OP_STORE:   std::snprintf(buf, sizeof buf, "[x%d] = x%d", d, b); break;
    case OP_STOREC:  std::snprintf(buf, sizeof buf, "[%d] = x%d", d, b); break;
    case OP_MOV:     std::snprintf(buf, sizeof buf, "x%d = x%d", d, b); break;
    case OP_MOVC:    std::snprintf(buf, sizeof buf, "x%d = %d", d, b); break;
    case OP_SHLOR:   std::snprintf(buf, sizeof buf, "x%d = (x%d << 8) | 0x%02X", d, d, b); break;   /* EXT e03 */
    case OP_PRINT:   std::snprintf(buf, sizeof buf, "print x%d", b); break;
    case OP_VLOAD:   std::snprintf(buf, sizeof buf, "v%d = [x%d]", d, b); break;
    case OP_VLOADC:  std::snprintf(buf, sizeof buf, "v%d = [%d]", d, b); break;
    case OP_VSTORE:  std::snprintf(buf, sizeof buf, "[x%d] = v%d", d, b); break;
    case OP_VSTOREC: std::snprintf(buf, sizeof buf, "[%d] = v%d", d, b); break;
    default:         std::snprintf(buf, sizeof buf, "unknown opcode 0x%02X", op); break;
    }
    return buf;
}
