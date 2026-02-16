#pragma once
#include "nes/common.h"
#include <stdbool.h>

typedef struct Bus Bus;

typedef struct CPU6502 {
    Bus* bus;

    u16 pc;
    u8  a, x, y;
    u8  sp;
    u8  p;        // status flags
    u64 cycles;

    bool jammed;

    // External interrupt lines (latched by CPU step)
    bool nmi_pending;
    bool irq_pending;
} CPU6502;

// Status flags
enum {
    F_C = 1 << 0,
    F_Z = 1 << 1,
    F_I = 1 << 2,
    F_D = 1 << 3,
    F_B = 1 << 4,
    F_U = 1 << 5,
    F_V = 1 << 6,
    F_N = 1 << 7
};

bool CPU6502_Init(CPU6502* c, Bus* bus);
void CPU6502_Reset(CPU6502* c);
int  CPU6502_Step(CPU6502* c);

// Interrupt request lines
void CPU6502_RequestNMI(CPU6502* c);
void CPU6502_RequestIRQ(CPU6502* c);

// ops referenced by cpu_tables.c
void op_ILL(CPU6502* c, u16 addr, bool has_addr, bool page_cross);
void op_NOP(CPU6502* c, u16 addr, bool has_addr, bool page_cross);

// official ops
void op_ADC(CPU6502*, u16, bool, bool);
void op_AND(CPU6502*, u16, bool, bool);
void op_ASL(CPU6502*, u16, bool, bool);
void op_BCC(CPU6502*, u16, bool, bool);
void op_BCS(CPU6502*, u16, bool, bool);
void op_BEQ(CPU6502*, u16, bool, bool);
void op_BIT(CPU6502*, u16, bool, bool);
void op_BMI(CPU6502*, u16, bool, bool);
void op_BNE(CPU6502*, u16, bool, bool);
void op_BPL(CPU6502*, u16, bool, bool);
void op_BRK(CPU6502*, u16, bool, bool);
void op_BVC(CPU6502*, u16, bool, bool);
void op_BVS(CPU6502*, u16, bool, bool);
void op_CLC(CPU6502*, u16, bool, bool);
void op_CLD(CPU6502*, u16, bool, bool);
void op_CLI(CPU6502*, u16, bool, bool);
void op_CLV(CPU6502*, u16, bool, bool);
void op_CMP(CPU6502*, u16, bool, bool);
void op_CPX(CPU6502*, u16, bool, bool);
void op_CPY(CPU6502*, u16, bool, bool);
void op_DEC(CPU6502*, u16, bool, bool);
void op_DEX(CPU6502*, u16, bool, bool);
void op_DEY(CPU6502*, u16, bool, bool);
void op_EOR(CPU6502*, u16, bool, bool);
void op_INC(CPU6502*, u16, bool, bool);
void op_INX(CPU6502*, u16, bool, bool);
void op_INY(CPU6502*, u16, bool, bool);
void op_JMP(CPU6502*, u16, bool, bool);
void op_JSR(CPU6502*, u16, bool, bool);
void op_LDA(CPU6502*, u16, bool, bool);
void op_LDX(CPU6502*, u16, bool, bool);
void op_LDY(CPU6502*, u16, bool, bool);
void op_LSR(CPU6502*, u16, bool, bool);
void op_ORA(CPU6502*, u16, bool, bool);
void op_PHA(CPU6502*, u16, bool, bool);
void op_PHP(CPU6502*, u16, bool, bool);
void op_PLA(CPU6502*, u16, bool, bool);
void op_PLP(CPU6502*, u16, bool, bool);
void op_ROL(CPU6502*, u16, bool, bool);
void op_ROR(CPU6502*, u16, bool, bool);
void op_RTI(CPU6502*, u16, bool, bool);
void op_RTS(CPU6502*, u16, bool, bool);
void op_SBC(CPU6502*, u16, bool, bool);
void op_SEC(CPU6502*, u16, bool, bool);
void op_SED(CPU6502*, u16, bool, bool);
void op_SEI(CPU6502*, u16, bool, bool);
void op_STA(CPU6502*, u16, bool, bool);
void op_STX(CPU6502*, u16, bool, bool);
void op_STY(CPU6502*, u16, bool, bool);
void op_TAX(CPU6502*, u16, bool, bool);
void op_TAY(CPU6502*, u16, bool, bool);
void op_TSX(CPU6502*, u16, bool, bool);
void op_TXA(CPU6502*, u16, bool, bool);
void op_TXS(CPU6502*, u16, bool, bool);
void op_TYA(CPU6502*, u16, bool, bool);
