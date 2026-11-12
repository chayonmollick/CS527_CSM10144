/* CS527 mini-computer simulator — sizes, limits and opcodes shared by every module.
 * Change a size here and rebuild (make clean && make); nothing else hard-codes it.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---- processors and registers (Labs 1-4) ---- */
#define NP              4       /* number of processors (Lab 4) */
#define NUM_XREGS       256     /* integer registers x0..x255, 32 bits each */
#define NUM_VREGS       32      /* vector registers v0..v31 (Lab 3) */
#define VLEN            8       /* 32-bit lanes per vector register = 256 bits */

/* ---- memory and paging (Labs 5-6) ---- */
#define INSTR_SPACE     1024    /* logical instruction space per task, bytes (256 instructions) */
#define DATA_SPACE      4096    /* logical data space per task, bytes */
#define MEMSIZE         8192    /* physical memory, bytes */
#define PAGESIZE        512     /* page size = frame size, bytes */
#define NUM_INSTR_PAGES (INSTR_SPACE / PAGESIZE)                  /* 2 */
#define NUM_DATA_PAGES  (DATA_SPACE / PAGESIZE)                   /* 8 */
#define NUM_LOGICAL_PAGES  (NUM_INSTR_PAGES + NUM_DATA_PAGES)     /* 10 entries per page table */
#define NUM_PHYSICAL_PAGES (MEMSIZE / PAGESIZE)                   /* 16 frames, frame 0 reserved */
#define MAX_INSTRUCTIONS   (INSTR_SPACE / 4)                      /* 256 */

/* ---- operating system (Lab 4) ---- */
#define MAX_TASKS       64      /* process-table size */
#define TIME_SLICE      10      /* instructions per scheduler turn */
#define SLEEP_US        10      /* pause after every time slice, to watch execution */
#define MAX_STEPS       10000000L  /* a task that runs longer is killed (endless loop guard) */
#define LOG_FILE        "log.txt"  /* print output (Lab 4), opened in append mode */

/* ---- opcodes (first byte of every 4-byte instruction) ----
 * Format: <opcode> <dest> <operand1> <operand2>. "c" = constant 0..255.
 */
enum Opcode {
    OP_HALT   = 0x00,   /* end of program                        00 0 0 0 */
    OP_ADD    = 0x01,   /* xD = xA + xB                          01 D A B */
    OP_SUB    = 0x02,   /* xD = xA - xB                          02 D A B */
    OP_MUL    = 0x03,   /* xD = xA * xB                          03 D A B */
    OP_DIV    = 0x04,   /* xD = xA / xB  (signed)                04 D A B */
    OP_LOAD   = 0x05,   /* xD = [xA]                             05 D 0 A */
    OP_STORE  = 0x06,   /* [xA] = xS                             06 A 0 S */
    OP_MOV    = 0x07,   /* xD = xA                               07 D 0 A */
    OP_PRINT  = 0x08,   /* print xR  (Lab 4)                     08 0 0 R */
    OP_ADDI   = 0x09,   /* xD = xA + c                           09 D A c */
    OP_SUBI   = 0x0A,   /* xD = xA - c                           0A D A c */
    OP_MULI   = 0x0B,   /* xD = xA * c                           0B D A c */
    OP_DIVI   = 0x0C,   /* xD = xA / c                           0C D A c */
    OP_LOADC  = 0x0D,   /* xD = [c]   (handout says 0x0C, which clashes with DIVI) */
    OP_STOREC = 0x0E,   /* [c] = xS                              0E c 0 S */
    OP_MOVC   = 0x0F,   /* xD = c                                0F D 0 c */
    OP_BRANCH = 0x10,   /* 0x10 + condition: B<cond> .label      1k 0 0 offset */
    OP_VADD   = 0x21,   /* vD = vA + vB  (Lab 3)                 21 D A B */
    OP_VSUB   = 0x22,   /* vD = vA - vB                          22 D A B */
    OP_VMUL   = 0x23,   /* vD = vA * vB                          23 D A B */
    OP_VLOAD  = 0x25,   /* vD = [xA]  (8 words from xA)          25 D 0 A */
    OP_VSTORE = 0x26,   /* [xA] = vS                             26 A 0 S */
    OP_VADDI  = 0x29,   /* vD = vA + c                           29 D A c */
    OP_VSUBI  = 0x2A,   /* vD = vA - c                           2A D A c */
    OP_VMULI  = 0x2B,   /* vD = vA * c                           2B D A c */
    OP_VLOADC = 0x2C,   /* vD = [c]                              2C D 0 c */
    OP_VSTOREC= 0x2E,   /* [c] = vS                              2E c 0 S */
    OP_VADDX  = 0x31,   /* vD = vA + xB  (scalar register; the handout allows it but gives no opcode) */
    OP_VSUBX  = 0x32,   /* vD = vA - xB */
    OP_VMULX  = 0x33    /* vD = vA * xB */
};

/* Branch conditions (ARM order): BEQ = 0x10 and BAL = 0x1E, as in the handout's example. */
enum Condition {
    COND_EQ = 0, COND_NE, COND_CS, COND_CC, COND_MI, COND_PL, COND_VS, COND_VC,
    COND_HI, COND_LS, COND_GE, COND_LT, COND_GT, COND_LE, COND_AL, NUM_CONDITIONS
};

#endif
