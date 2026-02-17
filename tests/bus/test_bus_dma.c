#include "nes/bus.h"
#include <assert.h>
#include <stdio.h>

static void test_oam_dma_copy_and_stall_even_cycle(void)
{
    Bus b;
    assert(Bus_Init(&b, NULL));

    for (int i = 0; i < 256; i++) {
        b.ram[0x0200 + i] = (u8)i;
    }

    b.ppu.oam_addr = 0;
    b.cpu_cycle_parity = 0;

    Bus_CPUWrite(&b, 0x4014, 0x02);

    assert(b.dma_active);
    assert(b.dma_stall_cycles == 513);

    for (int i = 0; i < 256; i++) {
        assert(b.ppu.oam[i] == (u8)i);
    }

    for (int i = 0; i < 512; i++) {
        assert(Bus_DMATick(&b));
    }
    assert(b.dma_active);
    assert(b.dma_stall_cycles == 1);

    assert(Bus_DMATick(&b));
    assert(!b.dma_active);
    assert(b.dma_stall_cycles == 0);

    assert(!Bus_DMATick(&b));
}

static void test_oam_dma_stall_odd_cycle(void)
{
    Bus b;
    assert(Bus_Init(&b, NULL));

    b.cpu_cycle_parity = 1;
    Bus_CPUWrite(&b, 0x4014, 0x00);

    assert(b.dma_active);
    assert(b.dma_stall_cycles == 514);
}

int main(void)
{
    test_oam_dma_copy_and_stall_even_cycle();
    test_oam_dma_stall_odd_cycle();
    puts("bus dma: OK");
    return 0;
}
