#include "nes/bus.h"
#include "nes/cart.h"
#include <string.h>

enum {
    APU_FRAME_IRQ_PERIOD_CPU = 29830
};

static void latch_controllers(Bus* b)
{
    // NES controller latch: bit0=A ... bit7=Right (matches our NesInput mapping)
    b->controller_latch_p1 = b->input.p1;
    b->controller_shift_p1 = b->controller_latch_p1;
}

static void perform_oam_dma_copy(Bus* b, u8 page)
{
    // CPU writes page to $4014; DMA copies 256 bytes from page<<8 to PPU OAM.
    u16 base = (u16)((u16)page << 8);
    for (u16 i = 0; i < 256u; i++) {
        u8 v = Bus_CPURead(b, (u16)(base + i));
        b->ppu.oam[b->ppu.oam_addr] = v;
        b->ppu.oam_addr++;
    }
}

static void begin_oam_dma(Bus* b, u8 page)
{
    if (!b) return;

    b->dma_page = page;
    perform_oam_dma_copy(b, page);

    // CPU is stalled for 513 or 514 cycles, depending on current parity.
    // This is instruction-granularity timing (not per-microcycle exact), but
    // preserves the key side effect that DMA steals CPU time.
    b->dma_stall_cycles = (u16)(513u + (b->cpu_cycle_parity ? 1u : 0u));
    b->dma_active = true;
}

bool Bus_Init(Bus* b, Cart* cart)
{
    if (!b) return false;
    memset(b, 0, sizeof(*b));
    b->cart = cart;

    if (!PPU2C02_Init(&b->ppu, cart)) return false;

    Bus_Reset(b);
    return true;
}

void Bus_Reset(Bus* b)
{
    if (!b) return;

    memset(b->ram, 0, sizeof(b->ram));
    b->open_bus = 0;

    b->controller_latch_p1 = 0;
    b->controller_shift_p1 = 0;
    b->controller_strobe = false;

    b->dma_active = false;
    b->dma_stall_cycles = 0;
    b->dma_page = 0;
    b->cpu_cycle_parity = 0;

    b->apu_status = 0;
    b->apu_frame_counter = 0;
    b->apu_frame_irq_pending = false;
    b->apu_frame_irq_inhibit = false;
    b->apu_frame_divider = 0;

    b->input.p1 = 0;

    PPU2C02_Reset(&b->ppu);
}

void Bus_SetCart(Bus* b, Cart* cart)
{
    if (!b) return;
    b->cart = cart;
    PPU2C02_SetCart(&b->ppu, cart);
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

bool Bus_DMATick(Bus* b)
{
    if (!b || !b->dma_active) return false;

    if (b->dma_stall_cycles > 0) {
        b->dma_stall_cycles--;
    }

    if (b->dma_stall_cycles == 0) {
        b->dma_active = false;
    }

    return true;
}

bool Bus_APUTick(Bus* b)
{
    if (!b) return false;

    // Minimal frame-counter IRQ model:
    // - only active in 4-step mode (bit 7 = 0)
    // - inhibited by bit 6 of $4017
    // - roughly one IRQ pulse per frame-counter sequence
    b->apu_frame_divider++;
    if (b->apu_frame_divider >= (u32)APU_FRAME_IRQ_PERIOD_CPU) {
        b->apu_frame_divider = 0;

        bool five_step_mode = (b->apu_frame_counter & 0x80u) != 0;
        if (!five_step_mode && !b->apu_frame_irq_inhibit) {
            b->apu_frame_irq_pending = true;
        }
    }

    if (b->apu_frame_irq_pending && !b->apu_frame_irq_inhibit) {
        return true;
    }

    return false;
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
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        u8 v = PPU2C02_CPURead(&b->ppu, addr, b->open_bus);
        b->open_bus = v;
        return v;
    }

    // $4000-$4017: APU/IO
    if (addr >= 0x4000 && addr <= 0x4017) {
        // $4015: APU status
        if (addr == 0x4015) {
            u8 irq_bit = b->apu_frame_irq_pending ? 0x40u : 0x00u;
            u8 v = (u8)((b->open_bus & 0x20u) | irq_bit | (b->apu_status & 0x1Fu));
            b->apu_frame_irq_pending = false;
            b->open_bus = v;
            return v;
        }

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
        PPU2C02_CPUWrite(&b->ppu, addr, data);
        return;
    }

    // $4000-$4017: APU/IO
    if (addr >= 0x4000 && addr <= 0x4017) {
        // $4014: OAMDMA
        if (addr == 0x4014) {
            begin_oam_dma(b, data);
            return;
        }

        // $4015: APU status/control
        if (addr == 0x4015) {
            b->apu_status = (u8)(data & 0x1Fu);
            return;
        }

        // $4016: controller strobe
        if (addr == 0x4016) {
            bool old_strobe = b->controller_strobe;
            bool new_strobe = (data & 0x01u) != 0;
            b->controller_strobe = new_strobe;

            if (new_strobe) {
                // While high, latch continuously
                latch_controllers(b);
            } else if (old_strobe && !new_strobe) {
                // Latch once on high->low transition
                latch_controllers(b);
            }
            return;
        }

        // $4017: APU frame counter
        if (addr == 0x4017) {
            b->apu_frame_counter = data;
            b->apu_frame_irq_inhibit = (data & 0x40u) != 0;
            if (b->apu_frame_irq_inhibit) {
                b->apu_frame_irq_pending = false;
            }
            b->apu_frame_divider = 0;
            return;
        }

        // Other APU regs ignored for now
        return;
    }

    // $4020-$FFFF: cartridge space (mapper)
    if (b->cart) {
        (void)Cart_CPUWrite(b->cart, addr, data);
    }
}
