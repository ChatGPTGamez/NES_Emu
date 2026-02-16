#include "nes/ppu/ppu2c02.h"
#include "nes/cart.h"
#include "nes/mapper.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct TestCartCtx {
    Cart cart;
    Mapper mapper;
    u8 chr[0x2000];
} TestCartCtx;

static bool test_ppu_read(Mapper* m, u16 addr, u8* out)
{
    TestCartCtx* tc = (TestCartCtx*)m->cart;
    if (addr <= 0x1FFFu) {
        *out = tc->chr[addr & 0x1FFFu];
        return true;
    }
    return false;
}

static bool test_ppu_write(Mapper* m, u16 addr, u8 data)
{
    TestCartCtx* tc = (TestCartCtx*)m->cart;
    if (addr <= 0x1FFFu) {
        tc->chr[addr & 0x1FFFu] = data;
        return true;
    }
    return false;
}

static PPU2C02 make_ppu_with_test_cart(TestCartCtx* tc)
{
    memset(tc, 0, sizeof(*tc));
    tc->mapper.cart = &tc->cart;
    tc->mapper.ppu_read = test_ppu_read;
    tc->mapper.ppu_write = test_ppu_write;
    tc->cart.mapper = &tc->mapper;
    tc->cart.info.mirroring = NES_MIRROR_HORIZONTAL;

    PPU2C02 p;
    assert(PPU2C02_Init(&p, &tc->cart));
    return p;
}

static void test_ppustatus_read_clears_vblank(void)
{
    PPU2C02 p;
    assert(PPU2C02_Init(&p, NULL));

    p.status = 0xE0u;
    p.w = true;

    u8 v = PPU2C02_CPURead(&p, 0x2002, 0x1Fu);
    assert((v & 0xE0u) == 0xE0u);
    assert((p.status & 0x80u) == 0u);
    assert(!p.w);
}

static void test_ppuaddr_ppudata_increment_1(void)
{
    PPU2C02 p;
    assert(PPU2C02_Init(&p, NULL));

    PPU2C02_CPUWrite(&p, 0x2006, 0x20);
    PPU2C02_CPUWrite(&p, 0x2006, 0x00);
    assert(p.v == 0x2000u);

    PPU2C02_CPUWrite(&p, 0x2007, 0xAA);
    assert(p.nametables[0] == 0xAAu);
    assert(p.v == 0x2001u);
}

static void test_ppuctrl_increment_32_mode(void)
{
    PPU2C02 p;
    assert(PPU2C02_Init(&p, NULL));

    PPU2C02_CPUWrite(&p, 0x2000, 0x04);
    PPU2C02_CPUWrite(&p, 0x2006, 0x20);
    PPU2C02_CPUWrite(&p, 0x2006, 0x00);

    PPU2C02_CPUWrite(&p, 0x2007, 0x55);
    assert(p.v == 0x2020u);
}

static void test_vblank_sets_frame_and_nmi(void)
{
    PPU2C02 p;
    assert(PPU2C02_Init(&p, NULL));

    PPU2C02_CPUWrite(&p, 0x2000, 0x80);

    p.scanline = 241;
    p.cycle = 1;

    PPU2C02_Clock(&p);

    assert((p.status & 0x80u) != 0u);
    assert(PPU2C02_PollNMI(&p));

    while (!PPU2C02_FrameComplete(&p)) {
        PPU2C02_Clock(&p);
    }
}

static void test_visible_dot_render_uses_backdrop_when_bg_disabled(void)
{
    PPU2C02 p;
    assert(PPU2C02_Init(&p, NULL));

    p.palette[0] = 0x01u;
    p.scanline = 0;
    p.cycle = 1;
    p.mask = 0x00u;

    PPU2C02_Clock(&p);
    assert(p.fb[0] == 0xFF001E74u);
}

static void test_scroll_copy_x_on_cycle_257(void)
{
    PPU2C02 p;
    assert(PPU2C02_Init(&p, NULL));

    p.mask = 0x08u;

    PPU2C02_CPUWrite(&p, 0x2006, 0x04);
    PPU2C02_CPUWrite(&p, 0x2006, 0x05);

    p.v = 0x0000u;

    p.scanline = 0;
    p.cycle = 257;
    PPU2C02_Clock(&p);

    assert((p.v & 0x041Fu) == (p.t & 0x041Fu));
}

static void test_sprite_renders_over_backdrop(void)
{
    TestCartCtx tc;
    PPU2C02 p = make_ppu_with_test_cart(&tc);

    p.mask = 0x10u;
    p.palette[0] = 0x01u;
    p.palette[0x11] = 0x20u;

    p.oam[0] = 9;
    p.oam[1] = 0;
    p.oam[2] = 0;
    p.oam[3] = 10;

    tc.chr[0x0000] = 0x80u;

    p.scanline = 10;
    p.cycle = 11;
    PPU2C02_Clock(&p);

    assert(p.fb[10 * PPU_FB_W + 10] == 0xFFECEEECu);
}

static void test_sprite0_hit_sets_status(void)
{
    TestCartCtx tc;
    PPU2C02 p = make_ppu_with_test_cart(&tc);

    p.mask = 0x1Eu;
    p.palette[0] = 0x01u;
    p.palette[0x11] = 0x20u;
    p.palette[0x01] = 0x30u;

    p.nametables[0] = 0;
    tc.chr[0x0000] = 0x80u;

    p.oam[0] = 0;
    p.oam[1] = 0;
    p.oam[2] = 0;
    p.oam[3] = 0;

    p.v = 0;
    p.scanline = 1;
    p.cycle = 1;
    PPU2C02_Clock(&p);

    assert((p.status & 0x40u) != 0u);
}

int main(void)
{
    test_ppustatus_read_clears_vblank();
    test_ppuaddr_ppudata_increment_1();
    test_ppuctrl_increment_32_mode();
    test_vblank_sets_frame_and_nmi();
    test_visible_dot_render_uses_backdrop_when_bg_disabled();
    test_scroll_copy_x_on_cycle_257();
    test_sprite_renders_over_backdrop();
    test_sprite0_hit_sets_status();
    puts("ppu smoke: OK");
    return 0;
}
