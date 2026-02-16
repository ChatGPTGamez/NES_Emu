#include "nes/ppu/ppu2c02.h"
#include "nes/cart.h"
#include <string.h>

enum {
    PPUCTRL_VRAM_INC = 1u << 2,
    PPUCTRL_NMI      = 1u << 7,

    PPUSTATUS_VBLANK = 1u << 7
};

static inline u16 mirror_nametable_addr(const PPU2C02* p, u16 addr)
{
    u16 nt = (u16)((addr - 0x2000u) & 0x0FFFu);
    u16 table = (u16)(nt / 0x0400u);
    u16 off = (u16)(nt & 0x03FFu);

    NesMirroring m = NES_MIRROR_HORIZONTAL;
    if (p->cart) m = p->cart->info.mirroring;

    switch (m) {
        case NES_MIRROR_VERTICAL:
            // NT0=0, NT1=1, NT2=0, NT3=1
            return (u16)(((table & 1u) * 0x0400u) + off);

        case NES_MIRROR_FOURSCREEN:
            // We only have 2KB local memory for now; fall back to vertical mapping.
            return (u16)(((table & 1u) * 0x0400u) + off);

        case NES_MIRROR_HORIZONTAL:
        default:
            // NT0=0, NT1=0, NT2=1, NT3=1
            return (u16)((((table >> 1) & 1u) * 0x0400u) + off);
    }
}

static inline u16 mirror_palette_addr(u16 addr)
{
    u16 p = (u16)(addr & 0x001Fu);
    if (p == 0x10u) p = 0x00u;
    if (p == 0x14u) p = 0x04u;
    if (p == 0x18u) p = 0x08u;
    if (p == 0x1Cu) p = 0x0Cu;
    return p;
}

static u8 ppu_mem_read(PPU2C02* p, u16 addr)
{
    addr &= 0x3FFFu;

    if (addr <= 0x1FFFu) {
        u8 v = 0;
        if (p->cart && Cart_PPURead(p->cart, addr, &v)) return v;
        return 0;
    }

    if (addr <= 0x3EFFu) {
        u16 off = mirror_nametable_addr(p, addr);
        return p->nametables[off];
    }

    return p->palette[mirror_palette_addr(addr)];
}

static void ppu_mem_write(PPU2C02* p, u16 addr, u8 data)
{
    addr &= 0x3FFFu;

    if (addr <= 0x1FFFu) {
        if (p->cart) {
            (void)Cart_PPUWrite(p->cart, addr, data);
        }
        return;
    }

    if (addr <= 0x3EFFu) {
        u16 off = mirror_nametable_addr(p, addr);
        p->nametables[off] = data;
        return;
    }

    p->palette[mirror_palette_addr(addr)] = data;
}

static void maybe_raise_nmi(PPU2C02* p)
{
    if ((p->status & PPUSTATUS_VBLANK) && (p->ctrl & PPUCTRL_NMI)) {
        p->nmi_pending = true;
    }
}

bool PPU2C02_Init(PPU2C02* p, Cart* cart)
{
    if (!p) return false;
    memset(p, 0, sizeof(*p));
    p->cart = cart;
    p->scanline = 0;
    return true;
}

void PPU2C02_Reset(PPU2C02* p)
{
    if (!p) return;

    p->ctrl = 0;
    p->mask = 0;
    p->status = 0;
    p->oam_addr = 0;

    p->v = 0;
    p->t = 0;
    p->x = 0;
    p->w = false;
    p->read_buffer = 0;

    p->cycle = 0;
    p->scanline = 0;
    p->frame_complete = false;
    p->nmi_pending = false;

    memset(p->nametables, 0, sizeof(p->nametables));
    memset(p->palette, 0, sizeof(p->palette));
    memset(p->oam, 0, sizeof(p->oam));
    memset(p->fb, 0, sizeof(p->fb));
}

void PPU2C02_SetCart(PPU2C02* p, Cart* cart)
{
    if (!p) return;
    p->cart = cart;
}

u8 PPU2C02_CPURead(PPU2C02* p, u16 addr, u8 open_bus)
{
    if (!p) return 0;

    u8 reg = (u8)(addr & 7u);

    switch (reg) {
        case 2: {
            u8 v = (u8)((p->status & 0xE0u) | (open_bus & 0x1Fu));
            p->status = (u8)(p->status & (u8)~PPUSTATUS_VBLANK);
            p->w = false;
            return v;
        }

        case 4:
            return p->oam[p->oam_addr];

        case 7: {
            u8 data = ppu_mem_read(p, p->v);
            u8 out;
            if ((p->v & 0x3FFFu) < 0x3F00u) {
                out = p->read_buffer;
                p->read_buffer = data;
            } else {
                out = data;
                p->read_buffer = ppu_mem_read(p, (u16)(p->v - 0x1000u));
            }

            p->v = (u16)(p->v + ((p->ctrl & PPUCTRL_VRAM_INC) ? 32u : 1u));
            return out;
        }

        default:
            return open_bus;
    }
}

void PPU2C02_CPUWrite(PPU2C02* p, u16 addr, u8 data)
{
    if (!p) return;

    u8 reg = (u8)(addr & 7u);

    switch (reg) {
        case 0: {
            bool old_nmi = (p->ctrl & PPUCTRL_NMI) != 0;
            p->ctrl = data;
            p->t = (u16)((p->t & 0xF3FFu) | ((u16)(data & 0x03u) << 10));
            bool new_nmi = (p->ctrl & PPUCTRL_NMI) != 0;
            if (!old_nmi && new_nmi) maybe_raise_nmi(p);
        } break;

        case 1:
            p->mask = data;
            break;

        case 3:
            p->oam_addr = data;
            break;

        case 4:
            p->oam[p->oam_addr] = data;
            p->oam_addr++;
            break;

        case 5:
            if (!p->w) {
                p->x = (u8)(data & 0x07u);
                p->t = (u16)((p->t & 0xFFE0u) | ((u16)data >> 3));
                p->w = true;
            } else {
                p->t = (u16)((p->t & 0x8C1Fu) |
                             (((u16)(data & 0x07u)) << 12) |
                             (((u16)(data & 0xF8u)) << 2));
                p->w = false;
            }
            break;

        case 6:
            if (!p->w) {
                p->t = (u16)((p->t & 0x00FFu) | (((u16)(data & 0x3Fu)) << 8));
                p->w = true;
            } else {
                p->t = (u16)((p->t & 0xFF00u) | data);
                p->v = p->t;
                p->w = false;
            }
            break;

        case 7:
            ppu_mem_write(p, p->v, data);
            p->v = (u16)(p->v + ((p->ctrl & PPUCTRL_VRAM_INC) ? 32u : 1u));
            break;

        default:
            break;
    }
}

void PPU2C02_Clock(PPU2C02* p)
{
    if (!p) return;

    // VBlank start at scanline 241, dot 1
    if (p->scanline == 241 && p->cycle == 1) {
        p->status = (u8)(p->status | PPUSTATUS_VBLANK);
        maybe_raise_nmi(p);
    }

    // Pre-render line clears vblank at dot 1
    if (p->scanline == -1 && p->cycle == 1) {
        p->status = (u8)(p->status & (u8)~PPUSTATUS_VBLANK);
    }

    p->cycle++;
    if (p->cycle > 340) {
        p->cycle = 0;
        p->scanline++;

        if (p->scanline > 260) {
            p->scanline = -1;
            p->frame_count++;
            p->frame_complete = true;
        }
    }
}

bool PPU2C02_PollNMI(PPU2C02* p)
{
    if (!p) return false;
    bool pending = p->nmi_pending;
    p->nmi_pending = false;
    return pending;
}

bool PPU2C02_FrameComplete(const PPU2C02* p)
{
    return p ? p->frame_complete : false;
}

void PPU2C02_ClearFrameComplete(PPU2C02* p)
{
    if (!p) return;
    p->frame_complete = false;
}
