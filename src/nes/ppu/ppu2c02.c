#include "nes/ppu/ppu2c02.h"
#include "nes/cart.h"
#include <string.h>

enum {
    PPUCTRL_VRAM_INC = 1u << 2,
    PPUCTRL_BG_TABLE = 1u << 4,
    PPUCTRL_NMI      = 1u << 7,

    PPUMASK_BG_LEFT  = 1u << 1,
    PPUMASK_BG_SHOW  = 1u << 3,

    PPUSTATUS_VBLANK = 1u << 7
};

static const u32 k_nes_rgb[64] = {
    0xFF545454u, 0xFF001E74u, 0xFF081090u, 0xFF300088u, 0xFF440064u, 0xFF5C0030u, 0xFF540400u, 0xFF3C1800u,
    0xFF202A00u, 0xFF083A00u, 0xFF004000u, 0xFF003C00u, 0xFF00323Cu, 0xFF000000u, 0xFF000000u, 0xFF000000u,
    0xFF989698u, 0xFF084CC4u, 0xFF3032ECu, 0xFF5C1EE4u, 0xFF8814B0u, 0xFFA01464u, 0xFF982220u, 0xFF783C00u,
    0xFF545A00u, 0xFF287200u, 0xFF087C00u, 0xFF007628u, 0xFF006678u, 0xFF000000u, 0xFF000000u, 0xFF000000u,
    0xFFECEEECu, 0xFF4C9AECu, 0xFF787CECu, 0xFFB062ECu, 0xFFE454ECu, 0xFFEC58B4u, 0xFFEC6A64u, 0xFFD48820u,
    0xFFA0AA00u, 0xFF74C400u, 0xFF4CD020u, 0xFF38CC6Cu, 0xFF38B4CCu, 0xFF3C3C3Cu, 0xFF000000u, 0xFF000000u,
    0xFFECEEECu, 0xFFA8CCECu, 0xFFBCBCECu, 0xFFD4B2F4u, 0xFFECAEECu, 0xFFECAED4u, 0xFFECB4B0u, 0xFFE4C490u,
    0xFFCCD278u, 0xFFB4DE78u, 0xFFA8E290u, 0xFF98E2B4u, 0xFFA0D6E4u, 0xFFA0A2A0u, 0xFF000000u, 0xFF000000u
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
    u16 pal = (u16)(addr & 0x001Fu);
    if (pal == 0x10u) pal = 0x00u;
    if (pal == 0x14u) pal = 0x04u;
    if (pal == 0x18u) pal = 0x08u;
    if (pal == 0x1Cu) pal = 0x0Cu;
    return pal;
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

static u32 palette_color(PPU2C02* p, u8 pal_index)
{
    u8 entry = ppu_mem_read(p, (u16)(0x3F00u + pal_index));
    return k_nes_rgb[entry & 0x3Fu];
}

static void inc_coarse_x(PPU2C02* p)
{
    if ((p->v & 0x001Fu) == 31u) {
        p->v &= (u16)~0x001Fu;
        p->v ^= 0x0400u;
    } else {
        p->v++;
    }
}

static void inc_y(PPU2C02* p)
{
    if ((p->v & 0x7000u) != 0x7000u) {
        p->v += 0x1000u;
        return;
    }

    p->v &= (u16)~0x7000u;
    u16 y = (u16)((p->v & 0x03E0u) >> 5);

    if (y == 29u) {
        y = 0u;
        p->v ^= 0x0800u;
    } else if (y == 31u) {
        y = 0u;
    } else {
        y++;
    }

    p->v = (u16)((p->v & (u16)~0x03E0u) | (u16)(y << 5));
}

static void copy_x(PPU2C02* p)
{
    p->v = (u16)((p->v & (u16)~0x041Fu) | (p->t & 0x041Fu));
}

static void copy_y(PPU2C02* p)
{
    p->v = (u16)((p->v & (u16)~0x7BE0u) | (p->t & 0x7BE0u));
}

static u8 bg_palette_index_at(PPU2C02* p, u16 v, u8 fine_x)
{
    u16 nt_addr = (u16)(0x2000u | (v & 0x0FFFu));
    u8 tile_id = ppu_mem_read(p, nt_addr);

    u16 attr_addr = (u16)(0x23C0u | (v & 0x0C00u) | ((v >> 4) & 0x38u) | ((v >> 2) & 0x07u));
    u8 attr = ppu_mem_read(p, attr_addr);

    u8 quadrant_shift = (u8)(((v >> 4) & 4u) | (v & 2u));
    u8 pal_hi = (u8)((attr >> quadrant_shift) & 0x03u);

    u8 fine_y = (u8)((v >> 12) & 0x07u);
    u16 patt_addr = (u16)(((p->ctrl & PPUCTRL_BG_TABLE) ? 0x1000u : 0x0000u)
                          + (u16)tile_id * 16u + fine_y);

    u8 plane0 = ppu_mem_read(p, patt_addr);
    u8 plane1 = ppu_mem_read(p, (u16)(patt_addr + 8u));

    u8 bit = (u8)(7u - (fine_x & 7u));
    u8 lo = (u8)((plane0 >> bit) & 1u);
    u8 hi = (u8)((plane1 >> bit) & 1u);
    u8 px = (u8)((hi << 1) | lo);

    if (px == 0) return 0u;
    return (u8)(((pal_hi << 2) | px) & 0x0Fu);
}

static void render_visible_dot(PPU2C02* p)
{
    int x = p->cycle - 1;
    int y = p->scanline;
    if (x < 0 || x >= PPU_FB_W || y < 0 || y >= PPU_FB_H) return;

    bool show_bg = (p->mask & PPUMASK_BG_SHOW) != 0;
    bool show_left_bg = (p->mask & PPUMASK_BG_LEFT) != 0;

    u8 pal_index = 0u;
    if (show_bg && (show_left_bg || x >= 8)) {
        u8 fine_x = (u8)(((u8)x + p->x) & 7u);
        pal_index = bg_palette_index_at(p, p->v, fine_x);
    }

    p->fb[y * PPU_FB_W + x] = palette_color(p, pal_index);
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

    bool rendering = (p->mask & PPUMASK_BG_SHOW) != 0;
    bool visible_scanline = (p->scanline >= 0 && p->scanline < 240);
    bool prerender_scanline = (p->scanline == -1);

    if (visible_scanline && p->cycle >= 1 && p->cycle <= 256) {
        render_visible_dot(p);
    }

    // VBlank start at scanline 241, dot 1
    if (p->scanline == 241 && p->cycle == 1) {
        p->status = (u8)(p->status | PPUSTATUS_VBLANK);
        maybe_raise_nmi(p);
    }

    // Pre-render line clears vblank at dot 1
    if (prerender_scanline && p->cycle == 1) {
        p->status = (u8)(p->status & (u8)~PPUSTATUS_VBLANK);
    }

    // Background address progression for visible/prerender lines.
    if (rendering && (visible_scanline || prerender_scanline)) {
        if (p->cycle >= 1 && p->cycle <= 256) {
            if ((p->cycle & 7) == 0) {
                inc_coarse_x(p);
            }
            if (p->cycle == 256) {
                inc_y(p);
            }
        }

        if (p->cycle == 257) {
            copy_x(p);
        }

        if (prerender_scanline && p->cycle >= 280 && p->cycle <= 304) {
            copy_y(p);
        }
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
