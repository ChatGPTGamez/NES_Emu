#pragma once
#include "nes/common.h"
#include "nes/input.h"
#include "nes/cart.h"
#include "nes/bus.h"
#include "nes/cpu/cpu6502.h"

enum {
    NES_FB_W = 256,
    NES_FB_H = 240
};

typedef struct Nes {
    Cart cart;
    Bus  bus;
    CPU6502 cpu;

    // Framebuffer (ARGB8888)
    u32 fb[NES_FB_W * NES_FB_H];

    u64 frame_count;
    NesInput input;
} Nes;

bool NES_Init(Nes* n);
void NES_Destroy(Nes* n);

bool NES_LoadROM(Nes* n, const char* path);
void NES_Reset(Nes* n);

void NES_RunFrame(Nes* n);

static inline const u32* NES_Framebuffer(const Nes* n) { return n ? n->fb : NULL; }
