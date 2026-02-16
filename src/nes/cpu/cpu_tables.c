#include "nes/cpu/cpu_tables.h"
#include "nes/cpu/cpu6502.h"

// Forward decls of all ops (implemented in cpu6502.c)
#define OP(name) void op_##name(CPU6502*, u16, bool, bool)
OP(ILL);
OP(NOP);

OP(ADC); OP(AND); OP(ASL); OP(BCC); OP(BCS); OP(BEQ); OP(BIT); OP(BMI); OP(BNE); OP(BPL);
OP(BRK); OP(BVC); OP(BVS);
OP(CLC); OP(CLD); OP(CLI); OP(CLV);
OP(CMP); OP(CPX); OP(CPY);
OP(DEC); OP(DEX); OP(DEY);
OP(EOR);
OP(INC); OP(INX); OP(INY);
OP(JMP); OP(JSR);
OP(LDA); OP(LDX); OP(LDY);
OP(LSR);
OP(ORA);
OP(PHA); OP(PHP); OP(PLA); OP(PLP);
OP(ROL); OP(ROR);
OP(RTI); OP(RTS);
OP(SBC); OP(SEC); OP(SED); OP(SEI);
OP(STA); OP(STX); OP(STY);
OP(TAX); OP(TAY); OP(TSX); OP(TXA); OP(TXS); OP(TYA);

#undef OP

// Default entry
#define E(name, mode, cyc) { #name, (mode), op_##name, (u8)(cyc) }
#define X() E(ILL, AM_IMP, 2)

const OpInfo g_op_table[256] = {
    // 0x00
    E(BRK, AM_IMP, 7), E(ORA, AM_IZX, 6), X(), X(), X(), E(ORA, AM_ZP, 3), E(ASL, AM_ZP, 5), X(),
    E(PHP, AM_IMP, 3), E(ORA, AM_IMM, 2), E(ASL, AM_ACC, 2), X(), X(), E(ORA, AM_ABS, 4), E(ASL, AM_ABS, 6), X(),

    // 0x10
    E(BPL, AM_REL, 2), E(ORA, AM_IZY, 5), X(), X(), X(), E(ORA, AM_ZPX, 4), E(ASL, AM_ZPX, 6), X(),
    E(CLC, AM_IMP, 2), E(ORA, AM_ABY, 4), X(), X(), X(), E(ORA, AM_ABX, 4), E(ASL, AM_ABX, 7), X(),

    // 0x20
    E(JSR, AM_ABS, 6), E(AND, AM_IZX, 6), X(), X(), E(BIT, AM_ZP, 3), E(AND, AM_ZP, 3), E(ROL, AM_ZP, 5), X(),
    E(PLP, AM_IMP, 4), E(AND, AM_IMM, 2), E(ROL, AM_ACC, 2), X(), E(BIT, AM_ABS, 4), E(AND, AM_ABS, 4), E(ROL, AM_ABS, 6), X(),

    // 0x30
    E(BMI, AM_REL, 2), E(AND, AM_IZY, 5), X(), X(), X(), E(AND, AM_ZPX, 4), E(ROL, AM_ZPX, 6), X(),
    E(SEC, AM_IMP, 2), E(AND, AM_ABY, 4), X(), X(), X(), E(AND, AM_ABX, 4), E(ROL, AM_ABX, 7), X(),

    // 0x40
    E(RTI, AM_IMP, 6), E(EOR, AM_IZX, 6), X(), X(), X(), E(EOR, AM_ZP, 3), E(LSR, AM_ZP, 5), X(),
    E(PHA, AM_IMP, 3), E(EOR, AM_IMM, 2), E(LSR, AM_ACC, 2), X(), E(JMP, AM_ABS, 3), E(EOR, AM_ABS, 4), E(LSR, AM_ABS, 6), X(),

    // 0x50
    E(BVC, AM_REL, 2), E(EOR, AM_IZY, 5), X(), X(), X(), E(EOR, AM_ZPX, 4), E(LSR, AM_ZPX, 6), X(),
    E(CLI, AM_IMP, 2), E(EOR, AM_ABY, 4), X(), X(), X(), E(EOR, AM_ABX, 4), E(LSR, AM_ABX, 7), X(),

    // 0x60
    E(RTS, AM_IMP, 6), E(ADC, AM_IZX, 6), X(), X(), X(), E(ADC, AM_ZP, 3), E(ROR, AM_ZP, 5), X(),
    E(PLA, AM_IMP, 4), E(ADC, AM_IMM, 2), E(ROR, AM_ACC, 2), X(), E(JMP, AM_IND, 5), E(ADC, AM_ABS, 4), E(ROR, AM_ABS, 6), X(),

    // 0x70
    E(BVS, AM_REL, 2), E(ADC, AM_IZY, 5), X(), X(), X(), E(ADC, AM_ZPX, 4), E(ROR, AM_ZPX, 6), X(),
    E(SEI, AM_IMP, 2), E(ADC, AM_ABY, 4), X(), X(), X(), E(ADC, AM_ABX, 4), E(ROR, AM_ABX, 7), X(),

    // 0x80
    X(), E(STA, AM_IZX, 6), X(), X(), E(STY, AM_ZP, 3), E(STA, AM_ZP, 3), E(STX, AM_ZP, 3), X(),
    E(DEY, AM_IMP, 2), X(), E(TXA, AM_IMP, 2), X(), E(STY, AM_ABS, 4), E(STA, AM_ABS, 4), E(STX, AM_ABS, 4), X(),

    // 0x90
    E(BCC, AM_REL, 2), E(STA, AM_IZY, 6), X(), X(), E(STY, AM_ZPX, 4), E(STA, AM_ZPX, 4), E(STX, AM_ZPY, 4), X(),
    E(TYA, AM_IMP, 2), E(STA, AM_ABY, 5), E(TXS, AM_IMP, 2), X(), X(), E(STA, AM_ABX, 5), X(), X(),

    // 0xA0
    E(LDY, AM_IMM, 2), E(LDA, AM_IZX, 6), E(LDX, AM_IMM, 2), X(), E(LDY, AM_ZP, 3), E(LDA, AM_ZP, 3), E(LDX, AM_ZP, 3), X(),
    E(TAY, AM_IMP, 2), E(LDA, AM_IMM, 2), E(TAX, AM_IMP, 2), X(), E(LDY, AM_ABS, 4), E(LDA, AM_ABS, 4), E(LDX, AM_ABS, 4), X(),

    // 0xB0
    E(BCS, AM_REL, 2), E(LDA, AM_IZY, 5), X(), X(), E(LDY, AM_ZPX, 4), E(LDA, AM_ZPX, 4), E(LDX, AM_ZPY, 4), X(),
    E(CLV, AM_IMP, 2), E(LDA, AM_ABY, 4), E(TSX, AM_IMP, 2), X(), E(LDY, AM_ABX, 4), E(LDA, AM_ABX, 4), E(LDX, AM_ABY, 4), X(),

    // 0xC0
    E(CPY, AM_IMM, 2), E(CMP, AM_IZX, 6), X(), X(), E(CPY, AM_ZP, 3), E(CMP, AM_ZP, 3), E(DEC, AM_ZP, 5), X(),
    E(INY, AM_IMP, 2), E(CMP, AM_IMM, 2), E(DEX, AM_IMP, 2), X(), E(CPY, AM_ABS, 4), E(CMP, AM_ABS, 4), E(DEC, AM_ABS, 6), X(),

    // 0xD0
    E(BNE, AM_REL, 2), E(CMP, AM_IZY, 5), X(), X(), X(), E(CMP, AM_ZPX, 4), E(DEC, AM_ZPX, 6), X(),
    E(CLD, AM_IMP, 2), E(CMP, AM_ABY, 4), X(), X(), X(), E(CMP, AM_ABX, 4), E(DEC, AM_ABX, 7), X(),

    // 0xE0
    E(CPX, AM_IMM, 2), E(SBC, AM_IZX, 6), X(), X(), E(CPX, AM_ZP, 3), E(SBC, AM_ZP, 3), E(INC, AM_ZP, 5), X(),
    E(INX, AM_IMP, 2), E(SBC, AM_IMM, 2), E(NOP, AM_IMP, 2), X(), E(CPX, AM_ABS, 4), E(SBC, AM_ABS, 4), E(INC, AM_ABS, 6), X(),

    // 0xF0
    E(BEQ, AM_REL, 2), E(SBC, AM_IZY, 5), X(), X(), X(), E(SBC, AM_ZPX, 4), E(INC, AM_ZPX, 6), X(),
    E(SED, AM_IMP, 2), E(SBC, AM_ABY, 4), X(), X(), X(), E(SBC, AM_ABX, 4), E(INC, AM_ABX, 7), X(),
};

#undef E
#undef X
