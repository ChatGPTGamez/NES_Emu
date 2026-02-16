#pragma once
#include "nes/common.h"
#include "nes/ines.h"
#include <stdbool.h>

typedef struct Cart Cart;

enum {
    PPU_FB_W = 256,
    PPU_FB_H = 240
};

typedef struct PPU2C02 {
    Cart* cart;

    // PPU registers/state
    u8 ctrl;      // $2000
    u8 mask;      // $2001
    u8 status;    // $2002
    u8 oam_addr;  // $2003

    u16 v;        // current VRAM address
    u16 t;        // temporary VRAM address
    u8 x;         // fine X scroll
    bool w;       // write toggle for $2005/$2006

    u8 read_buffer; // PPUDATA buffered read

    // Timing
    int cycle;      // 0..340
    int scanline;   // -1..260
    u64 frame_count;
    bool frame_complete;

    // Interrupt line (latched edge)
    bool nmi_pending;

    // Internal memory
    u8 nametables[2048];
    u8 palette[32];
    u8 oam[256];

    // Per-scanline sprite evaluation cache (up to 8 visible sprites)
    u8 scanline_sprites[8];
    u8 scanline_sprite_count;
    bool scanline_has_sprite0;
    bool scanline_overflow;
    int sprite_eval_scanline;

    // Framebuffer (ARGB8888)
    u32 fb[PPU_FB_W * PPU_FB_H];
} PPU2C02;

bool PPU2C02_Init(PPU2C02* p, Cart* cart);
void PPU2C02_Reset(PPU2C02* p);
void PPU2C02_SetCart(PPU2C02* p, Cart* cart);

u8   PPU2C02_CPURead(PPU2C02* p, u16 addr, u8 open_bus);
void PPU2C02_CPUWrite(PPU2C02* p, u16 addr, u8 data);

void PPU2C02_Clock(PPU2C02* p);
bool PPU2C02_PollNMI(PPU2C02* p);
bool PPU2C02_FrameComplete(const PPU2C02* p);
void PPU2C02_ClearFrameComplete(PPU2C02* p);

static inline const u32* PPU2C02_Framebuffer(const PPU2C02* p)
{
    return p ? p->fb : NULL;
}
