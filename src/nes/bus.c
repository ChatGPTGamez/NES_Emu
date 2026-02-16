#include "nes/bus.h"
#include "nes/cart.h"
#include <string.h>

static void latch_controllers(Bus* b)
{
    // NES controller latch: bit0=A ... bit7=Right (matches our NesInput mapping)
    b->controller_latch_p1 = b->input.p1;
    b->controller_shift_p1 = b->controller_latch_p1;
}

bool Bus_Init(Bus* b, Cart* cart)
{
    if (!b) return false;
    memset(b, 0, sizeof(*b));
    b->cart = cart;
    Bus_Reset(b);
    return true;
}

void Bus_Reset(Bus* b)
{
    if (!b) return;
    memset(b->ram, 0, sizeof(b->ram));
    memset(b->ppu_regs, 0, sizeof(b->ppu_regs));
    b->open_bus = 0;

    b->controller_latch_p1 = 0;
    b->controller_shift_p1 = 0;
    b->controller_strobe = false;

    b->input.p1 = 0;
}

void Bus_SetCart(Bus* b, Cart* cart)
{
    if (!b) return;
    b->cart = cart;
}

void Bus_SetInput(Bus* b, NesInput input)
{
    if (!b) return;
    b->input = input;

    // If strobe is high, controller is continuously latched (behavior on real NES)
    if (b->controller_strobe) {
        latch_controllers(b);
    }
}

u8 Bus_CPURead(Bus* b, u16 addr)
{
    if (!b) return 0;

    // $0000-$1FFF: internal RAM (mirrored every 2KB)
    if (addr <= 0x1FFF) {
        u8 v = b->ram[addr & 0x07FFu];
        b->open_bus = v;
        return v;
    }

    // $2000-$3FFF: PPU regs (mirrored every 8 bytes)
    // NOTE: Stub only. Real PPU will replace this behavior.
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        u16 r = (u16)(0x2000u + (addr & 7u));
        u8 idx = (u8)(r & 7u);

        u8 v = b->ppu_regs[idx];

        // Typical special-case: $2002 (PPUSTATUS) read clears vblank in real HW.
        // We do nothing yet; just return stub content.
        b->open_bus = v;
        return v;
    }

    // $4000-$4017: APU/IO
    if (addr >= 0x4000 && addr <= 0x4017) {
        // $4016: controller port 1
        if (addr == 0x4016) {
            // If strobe high, always return current A button in bit0
            u8 bit;
            if (b->controller_strobe) {
                bit = (b->input.p1 & 0x01u) ? 1u : 0u;
            } else {
                bit = (b->controller_shift_p1 & 0x01u) ? 1u : 0u;
                // Shift right; after 8 reads hardware returns 1s typically.
                b->controller_shift_p1 = (u8)((b->controller_shift_p1 >> 1) | 0x80u);
            }

            // Many games expect upper bits to be open bus-ish; we keep it simple.
            u8 v = (u8)((b->open_bus & 0xFEu) | bit);
            b->open_bus = v;
            return v;
        }

        // $4017: controller port 2 (not implemented -> open bus)
        if (addr == 0x4017) {
            return b->open_bus;
        }

        // Other APU/IO regs: return open bus for now
        return b->open_bus;
    }

    // $4020-$FFFF: cartridge space (mapper)
    if (b->cart) {
        u8 v;
        if (Cart_CPURead(b->cart, addr, &v)) {
            b->open_bus = v;
            return v;
        }
    }

    return b->open_bus;
}

void Bus_CPUWrite(Bus* b, u16 addr, u8 data)
{
    if (!b) return;

    b->open_bus = data;

    // $0000-$1FFF: internal RAM (mirrored)
    if (addr <= 0x1FFF) {
        b->ram[addr & 0x07FFu] = data;
        return;
    }

    // $2000-$3FFF: PPU regs (mirrored)
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        u16 r = (u16)(0x2000u + (addr & 7u));
        u8 idx = (u8)(r & 7u);
        b->ppu_regs[idx] = data;
        return;
    }

    // $4000-$4017: APU/IO
    if (addr >= 0x4000 && addr <= 0x4017) {
        // $4014: OAMDMA (PPU sprite DMA) - stub ignore for now
        if (addr == 0x4014) {
            return;
        }

        // $4016: controller strobe
        if (addr == 0x4016) {
            bool new_strobe = (data & 0x01u) != 0;
            b->controller_strobe = new_strobe;

            if (new_strobe) {
                // While high, latch continuously
                latch_controllers(b);
            } else {
                // On transition to low, latch once (common behavior)
                latch_controllers(b);
            }
            return;
        }

        // Other APU regs ignored for now
        return;
    }

    // $4020-$FFFF: cartridge space (mapper)
    if (b->cart) {
        (void)Cart_CPUWrite(b->cart, addr, data);
        return;
    }
}
