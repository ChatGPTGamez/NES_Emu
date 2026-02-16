#pragma once
#include "nes/common.h"
#include "nes/input.h"
#include <stdbool.h>

typedef struct Cart Cart;

typedef struct Bus {
    Cart* cart;

    // 2KB internal RAM ($0000-$07FF), mirrored to $1FFF
    u8 ram[2048];

    // PPU register stubs ($2000-$2007), mirrored to $3FFF
    u8 ppu_regs[8];

    // Open bus behavior (very simplified for now)
    u8 open_bus;

    // Controller I/O ($4016/$4017)
    u8 controller_latch_p1;
    u8 controller_shift_p1;
    bool controller_strobe;

    // Snapshot of current input (set each frame)
    NesInput input;
} Bus;

bool Bus_Init(Bus* b, Cart* cart);
void Bus_Reset(Bus* b);

void Bus_SetCart(Bus* b, Cart* cart);
void Bus_SetInput(Bus* b, NesInput input);

// CPU read/write (the 6502 will call these)
u8   Bus_CPURead(Bus* b, u16 addr);
void Bus_CPUWrite(Bus* b, u16 addr, u8 data);
