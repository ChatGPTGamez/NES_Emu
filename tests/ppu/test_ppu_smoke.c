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

int main(void)
{
    test_ppustatus_read_clears_vblank();
    test_ppuaddr_ppudata_increment_1();
    test_ppuctrl_increment_32_mode();
    test_vblank_sets_frame_and_nmi();
    puts("ppu smoke: OK");
    return 0;
}
