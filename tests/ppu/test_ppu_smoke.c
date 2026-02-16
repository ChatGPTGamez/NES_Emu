#include "nes/ppu/ppu2c02.h"
#include <assert.h>
#include <stdio.h>

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

    // Enable NMI
    PPU2C02_CPUWrite(&p, 0x2000, 0x80);

    // Position at vblank trigger point: scanline 241, cycle 1
    p.scanline = 241;
    p.cycle = 1;

    PPU2C02_Clock(&p);

    assert((p.status & 0x80u) != 0u);
    assert(PPU2C02_PollNMI(&p));

    // Ensure frame_complete eventually toggles at frame end.
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
    p.mask = 0x00u; // background disabled

    PPU2C02_Clock(&p);
    assert(p.fb[0] == 0xFF001E74u);
}

static void test_scroll_copy_x_on_cycle_257(void)
{
    PPU2C02 p;
    assert(PPU2C02_Init(&p, NULL));

    p.mask = 0x08u; // show background enables address progression

    // write t: coarse x=5, nt_x=1
    PPU2C02_CPUWrite(&p, 0x2006, 0x04);
    PPU2C02_CPUWrite(&p, 0x2006, 0x05);

    // force v to different coarse x / nt_x so copy_x is visible
    p.v = 0x0000u;

    p.scanline = 0;
    p.cycle = 257;
    PPU2C02_Clock(&p);

    assert((p.v & 0x041Fu) == (p.t & 0x041Fu));
}

int main(void)
{
    test_ppustatus_read_clears_vblank();
    test_ppuaddr_ppudata_increment_1();
    test_ppuctrl_increment_32_mode();
    test_vblank_sets_frame_and_nmi();
    test_visible_dot_render_uses_backdrop_when_bg_disabled();
    test_scroll_copy_x_on_cycle_257();
    puts("ppu smoke: OK");
    return 0;
}
