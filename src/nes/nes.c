#include "nes/nes.h"
#include "nes/log.h"
#include "nes/ppu/ppu2c02.h"
#include <string.h>

static void NES_Clock(Nes* n)
{
    if (!n) return;
    if (n->cpu.jammed) return;

    int cpu_cycles = CPU6502_Step(&n->cpu);
    if (cpu_cycles <= 0) return;

    for (int i = 0; i < cpu_cycles * 3; i++) {
        PPU2C02_Clock(&n->bus.ppu);
        if (PPU2C02_PollNMI(&n->bus.ppu)) {
            CPU6502_RequestNMI(&n->cpu);
        }
    }
}

bool NES_Init(Nes* n)
{
    if (!n) return false;
    memset(n, 0, sizeof(*n));

    if (!Cart_Init(&n->cart)) return false;
    if (!Bus_Init(&n->bus, &n->cart)) return false;
    if (!CPU6502_Init(&n->cpu, &n->bus)) return false;

    for (u32 i = 0; i < (u32)(NES_FB_W * NES_FB_H); i++) n->fb[i] = 0xFF000000u;
    return true;
}

void NES_Destroy(Nes* n)
{
    if (!n) return;
    Cart_Destroy(&n->cart);
}

bool NES_LoadROM(Nes* n, const char* path)
{
    if (!n || !path) return false;

    if (!Cart_LoadFromFile(&n->cart, path)) return false;

    Bus_SetCart(&n->bus, &n->cart);

    NES_LOGI("NES: ROM loaded OK (mapper %u)", n->cart.info.mapper);
    return true;
}

void NES_Reset(Nes* n)
{
    if (!n) return;

    n->frame_count = 0;

    Bus_Reset(&n->bus);
    CPU6502_Reset(&n->cpu);

    NES_LOGI("CPU reset: PC=%04X", n->cpu.pc);
}

void NES_RunFrame(Nes* n)
{
    if (!n) return;

    // Feed input to bus ($4016)
    Bus_SetInput(&n->bus, n->input);

    PPU2C02_ClearFrameComplete(&n->bus.ppu);

    // Frame execution is driven by the PPU frame boundary.
    while (!PPU2C02_FrameComplete(&n->bus.ppu) && !n->cpu.jammed) {
        NES_Clock(n);
    }

    // Present PPU-rendered framebuffer.
    memcpy(n->fb, n->bus.ppu.fb, sizeof(n->fb));

    n->frame_count++;
}
