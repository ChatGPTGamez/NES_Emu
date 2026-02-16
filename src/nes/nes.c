#include "nes/nes.h"
#include "nes/log.h"
#include <string.h>

static u32 pack_argb(u8 a, u8 r, u8 g, u8 b)
{
    return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
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

    // --- TEMP bring-up: execute a fixed number of CPU instructions per "frame"
    // This is NOT final timing. Just to prove reset code runs.
    for (int i = 0; i < 5000 && !n->cpu.jammed; i++) {
        CPU6502_Step(&n->cpu);
    }

    // Placeholder visuals: show whether CPU is alive or jammed
    bool jam = n->cpu.jammed;

    u8 base = jam ? 40u : 0u;
    u32 t = (u32)(n->frame_count & 255u);

    for (int y = 0; y < NES_FB_H; y++) {
        for (int x = 0; x < NES_FB_W; x++) {
            u8 r = (u8)(((x + (int)t) & 255) >> 2);
            u8 g = (u8)(((y + (int)(t >> 1)) & 255) >> 2);
            u8 b = (u8)(((x ^ y ^ (int)t) & 255) >> 2);

            // If jammed, tint red so it's obvious
            if (jam) {
                r = (u8)(r + 120u);
                g = (u8)(g + base);
                b = (u8)(b + base);
            }

            n->fb[y * NES_FB_W + x] = pack_argb(0xFF, r, g, b);
        }
    }

    // Draw a simple "PC bar" at top: length depends on low byte of PC
    int w = 16 + (n->cpu.pc & 0xFF);
    if (w > NES_FB_W - 16) w = NES_FB_W - 16;
    for (int y = 8; y < 16; y++) {
        for (int x = 8; x < 8 + w; x++) {
            n->fb[y * NES_FB_W + x] = jam ? 0xFFFF0000u : 0xFFFFFFFFu;
        }
    }

    n->frame_count++;
}
