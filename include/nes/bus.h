#pragma once
#include "nes/common.h"
#include "nes/input.h"
#include "nes/ppu/ppu2c02.h"
#include "nes/apu/apu2a03.h"
#include <stdbool.h>

typedef struct Cart Cart;

typedef struct Bus {
    Cart* cart;

    // 2KB internal RAM ($0000-$07FF), mirrored to $1FFF
    u8 ram[2048];

    // PPU core + register interface
    PPU2C02 ppu;

    // APU core
    APU2A03 apu;

    // Open bus behavior (very simplified for now)
    u8 open_bus;

    // Controller I/O ($4016/$4017)
    u8 controller_latch_p1;
    u8 controller_shift_p1;
    u8 controller_latch_p2;
    u8 controller_shift_p2;
    bool controller_strobe;

    // OAM DMA timing/model state
    bool dma_active;
    u16 dma_stall_cycles;
    u8 dma_page;

    // CPU-cycle parity tracker (0 = even, 1 = odd)
    u8 cpu_cycle_parity;

    // Snapshot of current input (set each frame)
    NesInput input;
} Bus;

bool Bus_Init(Bus* b, Cart* cart);
void Bus_Reset(Bus* b);

void Bus_SetCart(Bus* b, Cart* cart);
void Bus_SetInput(Bus* b, NesInput input);

bool Bus_DMATick(Bus* b);
bool Bus_APUTick(Bus* b);

// CPU read/write (the 6502 will call these)
u8   Bus_CPURead(Bus* b, u16 addr);
void Bus_CPUWrite(Bus* b, u16 addr, u8 data);
