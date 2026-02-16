#pragma once
#include "nes/common.h"
#include <stdbool.h>

typedef struct CPU6502 CPU6502;

typedef enum AddrMode {
    AM_IMP = 0, // implied
    AM_ACC,     // accumulator
    AM_IMM,     // immediate
    AM_ZP,      // zero page
    AM_ZPX,     // zero page,X
    AM_ZPY,     // zero page,Y
    AM_REL,     // relative
    AM_ABS,     // absolute
    AM_ABX,     // absolute,X
    AM_ABY,     // absolute,Y
    AM_IND,     // indirect (JMP only, with 6502 page bug)
    AM_IZX,     // (zp,X)
    AM_IZY      // (zp),Y
} AddrMode;

typedef void (*OpFunc)(CPU6502* c, u16 addr, bool has_addr, bool page_cross);

typedef struct OpInfo {
    const char* name;
    AddrMode mode;
    OpFunc fn;
    u8 cycles;
} OpInfo;

extern const OpInfo g_op_table[256];
