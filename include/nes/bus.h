#pragma once
#include "nes/common.h"
#include "nes/input.h"
#include "nes/ppu/ppu2c02.h"
#include <stdbool.h>

typedef struct Cart Cart;

typedef struct Bus {
    Cart* cart;

    // 2KB internal RAM ($0000-$07FF), mirrored to $1FFF
    u8 ram[2048];

    // PPU core + register interface
    PPU2C02 ppu;

    // Open bus behavior (very simplified for now)
    u8 open_bus;

    // Controller I/O ($4016/$4017)
    u8 controller_latch_p1;
    u8 controller_shift_p1;
    bool controller_strobe;

    // OAM DMA timing/model state
    bool dma_active;
    u16 dma_stall_cycles;
    u8 dma_page;

    // CPU-cycle parity tracker (0 = even, 1 = odd)
    u8 cpu_cycle_parity;

    // Minimal APU/frame-counter state
    u8 apu_status;               // $4015 write-side channel enable bits
    u8 apu_frame_counter;        // $4017 last written value
    bool apu_frame_irq_pending;  // frame IRQ flag (visible in $4015 bit 6)
    bool apu_frame_irq_inhibit;  // $4017 bit 6
    u32 apu_frame_divider;       // CPU-cycle divider for frame IRQ cadence

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
