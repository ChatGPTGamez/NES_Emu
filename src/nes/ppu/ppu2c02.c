#include "nes/ppu/ppu2c02.h"
#include "nes/cart.h"
#include <string.h>

enum {
    PPUCTRL_VRAM_INC  = 1u << 2,
    PPUCTRL_SPR_TABLE = 1u << 3,
    PPUCTRL_BG_TABLE  = 1u << 4,
    PPUCTRL_NMI       = 1u << 7,

    PPUMASK_BG_LEFT   = 1u << 1,
    PPUMASK_SPR_LEFT  = 1u << 2,
    PPUMASK_BG_SHOW   = 1u << 3,
    PPUMASK_SPR_SHOW  = 1u << 4,

    PPUSTATUS_SPROVERFLOW = 1u << 5,
    PPUSTATUS_SPR0HIT     = 1u << 6,
    PPUSTATUS_VBLANK      = 1u << 7
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
            return (u16)(((table & 1u) * 0x0400u) + off);

        case NES_MIRROR_FOURSCREEN:
            return (u16)(((table & 1u) * 0x0400u) + off);

        case NES_MIRROR_HORIZONTAL:
        default:
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

static void bg_shift(PPU2C02* p)
{
    p->bg_shifter_pat_lo <<= 1;
    p->bg_shifter_pat_hi <<= 1;
    p->bg_shifter_attr_lo <<= 1;
    p->bg_shifter_attr_hi <<= 1;
}

static void bg_load_shifters(PPU2C02* p)
{
    p->bg_shifter_pat_lo = (u16)((p->bg_shifter_pat_lo & 0xFF00u) | p->bg_next_tile_lsb);
    p->bg_shifter_pat_hi = (u16)((p->bg_shifter_pat_hi & 0xFF00u) | p->bg_next_tile_msb);

    u16 attr_lo = (p->bg_next_tile_attr & 0x01u) ? 0x00FFu : 0x0000u;
    u16 attr_hi = (p->bg_next_tile_attr & 0x02u) ? 0x00FFu : 0x0000u;
    p->bg_shifter_attr_lo = (u16)((p->bg_shifter_attr_lo & 0xFF00u) | attr_lo);
    p->bg_shifter_attr_hi = (u16)((p->bg_shifter_attr_hi & 0xFF00u) | attr_hi);
}

static void bg_fetch_step(PPU2C02* p)
{
    switch (p->cycle & 7) {
        case 1:
            bg_load_shifters(p);
            p->bg_next_tile_id = ppu_mem_read(p, (u16)(0x2000u | (p->v & 0x0FFFu)));
            break;

        case 3: {
            u16 attr_addr = (u16)(0x23C0u | (p->v & 0x0C00u) | ((p->v >> 4) & 0x38u) | ((p->v >> 2) & 0x07u));
            u8 attr = ppu_mem_read(p, attr_addr);
            if (p->v & 0x40u) attr >>= 4;
            if (p->v & 0x02u) attr >>= 2;
            p->bg_next_tile_attr = (u8)(attr & 0x03u);
        } break;

        case 5: {
            u8 fine_y = (u8)((p->v >> 12) & 0x07u);
            u16 patt_addr = (u16)(((p->ctrl & PPUCTRL_BG_TABLE) ? 0x1000u : 0x0000u)
                                  + (u16)p->bg_next_tile_id * 16u + fine_y);
            p->bg_next_tile_lsb = ppu_mem_read(p, patt_addr);
        } break;

        case 7: {
            u8 fine_y = (u8)((p->v >> 12) & 0x07u);
            u16 patt_addr = (u16)(((p->ctrl & PPUCTRL_BG_TABLE) ? 0x1000u : 0x0000u)
                                  + (u16)p->bg_next_tile_id * 16u + fine_y + 8u);
            p->bg_next_tile_msb = ppu_mem_read(p, patt_addr);
        } break;

        case 0:
            inc_coarse_x(p);
            break;

        default:
            break;
    }
}

static u8 bg_palette_index_from_shifters(const PPU2C02* p)
{
    u16 bit_mux = (u16)(0x8000u >> p->x);

    u8 p0 = (p->bg_shifter_pat_lo & bit_mux) ? 1u : 0u;
    u8 p1 = (p->bg_shifter_pat_hi & bit_mux) ? 1u : 0u;
    u8 px = (u8)((p1 << 1) | p0);
    if (px == 0u) return 0u;

    u8 a0 = (p->bg_shifter_attr_lo & bit_mux) ? 1u : 0u;
    u8 a1 = (p->bg_shifter_attr_hi & bit_mux) ? 1u : 0u;
    return (u8)(((a1 << 1) | a0) << 2 | px);
}

static void evaluate_scanline_sprites(PPU2C02* p, int y)
{
    p->scanline_sprite_count = 0;
    p->scanline_has_sprite0 = false;
    p->scanline_overflow = false;

    for (int i = 0; i < 64; i++) {
        int base = i * 4;
        int sy = (int)p->oam[base] + 1;
        if (y < sy || y >= sy + 8) continue;

        if (p->scanline_sprite_count < 8) {
            p->scanline_sprites[p->scanline_sprite_count++] = (u8)i;
            if (i == 0) p->scanline_has_sprite0 = true;
        } else {
            p->scanline_overflow = true;
            break;
        }
    }
}

static bool sprite_palette_index_at(PPU2C02* p, int x, int y,
                                    u8* out_pal_index, bool* out_behind_bg,
                                    bool* out_sprite0)
{
    for (int si = 0; si < (int)p->scanline_sprite_count; si++) {
        int i = (int)p->scanline_sprites[si];
        int base = i * 4;
        int sy = (int)p->oam[base] + 1;
        int tile_x = (int)p->oam[base + 3];

        if (x < tile_x || x >= tile_x + 8) continue;

        int row = y - sy;
        int col = x - tile_x;

        u8 attr = p->oam[base + 2];
        if (attr & 0x80u) row = 7 - row;
        if (attr & 0x40u) col = 7 - col;

        u8 tile = p->oam[base + 1];
        u16 patt_addr = (u16)(((p->ctrl & PPUCTRL_SPR_TABLE) ? 0x1000u : 0x0000u)
                              + (u16)tile * 16u + (u16)row);
        u8 plane0 = ppu_mem_read(p, patt_addr);
        u8 plane1 = ppu_mem_read(p, (u16)(patt_addr + 8u));

        u8 bit = (u8)(7 - col);
        u8 lo = (u8)((plane0 >> bit) & 1u);
        u8 hi = (u8)((plane1 >> bit) & 1u);
        u8 px = (u8)((hi << 1) | lo);

        if (px == 0) continue;

        *out_pal_index = (u8)(0x10u | ((attr & 0x03u) << 2) | px);
        *out_behind_bg = (attr & 0x20u) != 0;
        *out_sprite0 = (i == 0);
        return true;
    }

    return false;
}

static void render_visible_dot(PPU2C02* p)
{
    int x = p->cycle - 1;
    int y = p->scanline;
    if (x < 0 || x >= PPU_FB_W || y < 0 || y >= PPU_FB_H) return;

    bool show_bg = (p->mask & PPUMASK_BG_SHOW) != 0;
    bool show_spr = (p->mask & PPUMASK_SPR_SHOW) != 0;
    bool show_left_bg = (p->mask & PPUMASK_BG_LEFT) != 0;
    bool show_left_spr = (p->mask & PPUMASK_SPR_LEFT) != 0;

    u8 bg_pal_index = 0u;
    if (show_bg && (show_left_bg || x >= 8)) {
        bg_pal_index = bg_palette_index_from_shifters(p);
    }

    u8 spr_pal_index = 0u;
    bool spr_behind_bg = false;
    bool spr0 = false;
    bool spr_opaque = false;

    if (show_spr && (show_left_spr || x >= 8)) {
        spr_opaque = sprite_palette_index_at(p, x, y, &spr_pal_index, &spr_behind_bg, &spr0);
    }

    bool bg_opaque = bg_pal_index != 0u;

    if (spr0 && bg_opaque && spr_opaque && x < 255) {
        p->status |= PPUSTATUS_SPR0HIT;
    }

    u8 out_pal = 0u;
    if (bg_opaque && spr_opaque) {
        out_pal = spr_behind_bg ? bg_pal_index : spr_pal_index;
    } else if (spr_opaque) {
        out_pal = spr_pal_index;
    } else if (bg_opaque) {
        out_pal = bg_pal_index;
    }

    p->fb[y * PPU_FB_W + x] = palette_color(p, out_pal);
}

bool PPU2C02_Init(PPU2C02* p, Cart* cart)
{
    if (!p) return false;
    memset(p, 0, sizeof(*p));
    p->cart = cart;
    p->scanline = 0;
    p->sprite_eval_scanline = -2;
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

    p->bg_next_tile_id = 0;
    p->bg_next_tile_attr = 0;
    p->bg_next_tile_lsb = 0;
    p->bg_next_tile_msb = 0;
    p->bg_shifter_pat_lo = 0;
    p->bg_shifter_pat_hi = 0;
    p->bg_shifter_attr_lo = 0;
    p->bg_shifter_attr_hi = 0;

    p->scanline_sprite_count = 0;
    p->scanline_has_sprite0 = false;
    p->scanline_overflow = false;
    p->sprite_eval_scanline = -2;

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

    bool rendering = (p->mask & (PPUMASK_BG_SHOW | PPUMASK_SPR_SHOW)) != 0;
    bool visible_scanline = (p->scanline >= 0 && p->scanline < 240);
    bool prerender_scanline = (p->scanline == -1);

    if (visible_scanline && p->sprite_eval_scanline != p->scanline) {
        evaluate_scanline_sprites(p, p->scanline);
        p->sprite_eval_scanline = p->scanline;
        if (p->scanline_overflow) {
            p->status |= PPUSTATUS_SPROVERFLOW;
        }
    }

    if (visible_scanline && p->cycle >= 1 && p->cycle <= 256) {
        render_visible_dot(p);
    }

    if (p->scanline == 241 && p->cycle == 1) {
        p->status = (u8)(p->status | PPUSTATUS_VBLANK);
        maybe_raise_nmi(p);
    }

    if (prerender_scanline && p->cycle == 1) {
        p->status = (u8)(p->status & (u8)~(PPUSTATUS_VBLANK | PPUSTATUS_SPR0HIT | PPUSTATUS_SPROVERFLOW));
    }

    if (rendering && (visible_scanline || prerender_scanline)) {
        if ((p->cycle >= 2 && p->cycle <= 257) || (p->cycle >= 322 && p->cycle <= 337)) {
            bg_shift(p);
        }

        if ((p->cycle >= 1 && p->cycle <= 256) || (p->cycle >= 321 && p->cycle <= 336)) {
            bg_fetch_step(p);
        }

        if (p->cycle == 256) {
            inc_y(p);
        }

        if (p->cycle == 257) {
            copy_x(p);
            bg_load_shifters(p);
        }

        if (prerender_scanline && p->cycle >= 280 && p->cycle <= 304) {
            copy_y(p);
        }
    }

    p->cycle++;
    if (p->cycle > 340) {
        p->cycle = 0;
        p->scanline++;
        p->sprite_eval_scanline = -2;

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
