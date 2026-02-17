#include "nes/bus.h"
#include "nes/cart.h"
#include <string.h>

static const u8 k_apu_len_table[32] = {
    10, 254, 20, 2, 40, 4, 80, 6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

static void latch_controllers(Bus* b)
{
    // NES controller latch: bit0=A ... bit7=Right (matches our NesInput mapping)
    b->controller_latch_p1 = b->input.p1;
    b->controller_shift_p1 = b->controller_latch_p1;

    b->controller_latch_p2 = b->input.p2;
    b->controller_shift_p2 = b->controller_latch_p2;
}

static u8 read_controller_port(Bus* b, bool port2)
{
    u8 bit;
    if (b->controller_strobe) {
        u8 src = port2 ? b->input.p2 : b->input.p1;
        bit = (src & 0x01u) ? 1u : 0u;
    } else {
        if (port2) {
            bit = (b->controller_shift_p2 & 0x01u) ? 1u : 0u;
            b->controller_shift_p2 = (u8)((b->controller_shift_p2 >> 1) | 0x80u);
        } else {
            bit = (b->controller_shift_p1 & 0x01u) ? 1u : 0u;
            b->controller_shift_p1 = (u8)((b->controller_shift_p1 >> 1) | 0x80u);
        }
    }

    // Many games expect upper bits to be open bus-ish; keep previous upper bits.
    u8 v = (u8)((b->open_bus & 0xFEu) | bit);
    b->open_bus = v;
    return v;
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
    b->dma_stall_cycles = (u16)(513u + (b->cpu_cycle_parity ? 1u : 0u));
    b->dma_active = true;
}

static void apu_clock_half_frame(Bus* b)
{
    for (u8 ch = 0; ch < 4u; ch++) {
        if ((b->apu_status & (u8)(1u << ch)) && b->apu_length_ctr[ch] > 0u) {
            b->apu_length_ctr[ch]--;
        }
    }
}

static void apu_write_channel_len_reload(Bus* b, u16 addr, u8 data)
{
    // Writes to $4003/$4007/$400B/$400F load length counters from bits 7..3.
    if (addr == 0x4003u) {
        if (b->apu_status & 0x01u) b->apu_length_ctr[0] = k_apu_len_table[(data >> 3) & 0x1Fu];
        return;
    }
    if (addr == 0x4007u) {
        if (b->apu_status & 0x02u) b->apu_length_ctr[1] = k_apu_len_table[(data >> 3) & 0x1Fu];
        return;
    }
    if (addr == 0x400Bu) {
        if (b->apu_status & 0x04u) b->apu_length_ctr[2] = k_apu_len_table[(data >> 3) & 0x1Fu];
        return;
    }
    if (addr == 0x400Fu) {
        if (b->apu_status & 0x08u) b->apu_length_ctr[3] = k_apu_len_table[(data >> 3) & 0x1Fu];
        return;
    }
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
    b->controller_latch_p2 = 0;
    b->controller_shift_p2 = 0;
    b->controller_strobe = false;

    b->dma_active = false;
    b->dma_stall_cycles = 0;
    b->dma_page = 0;
    b->cpu_cycle_parity = 0;

    memset(b->apu_regs, 0, sizeof(b->apu_regs));
    b->apu_status = 0;
    b->apu_frame_counter = 0;
    b->apu_frame_irq_pending = false;
    b->apu_frame_irq_inhibit = false;
    b->apu_five_step_mode = false;
    b->apu_frame_cycle = 0;
    memset(b->apu_length_ctr, 0, sizeof(b->apu_length_ctr));

    b->input.p1 = 0;
    b->input.p2 = 0;

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

    // Minimal-but-structured frame sequencer timing.
    b->apu_frame_cycle++;

    if (!b->apu_five_step_mode) {
        if (b->apu_frame_cycle == 7457u || b->apu_frame_cycle == 14915u) {
            apu_clock_half_frame(b);
        }

        if (b->apu_frame_cycle >= 14915u) {
            if (!b->apu_frame_irq_inhibit) {
                b->apu_frame_irq_pending = true;
            }
            b->apu_frame_cycle = 0;
        }
    } else {
        if (b->apu_frame_cycle == 7457u || b->apu_frame_cycle == 14915u) {
            apu_clock_half_frame(b);
        }

        if (b->apu_frame_cycle >= 18641u) {
            b->apu_frame_cycle = 0;
        }
    }

    return b->apu_frame_irq_pending && !b->apu_frame_irq_inhibit;
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
            u8 status = 0;
            if (b->apu_length_ctr[0] > 0u) status |= 0x01u;
            if (b->apu_length_ctr[1] > 0u) status |= 0x02u;
            if (b->apu_length_ctr[2] > 0u) status |= 0x04u;
            if (b->apu_length_ctr[3] > 0u) status |= 0x08u;
            if (b->apu_status & 0x10u) status |= 0x10u; // DMC active approximated by enable

            u8 irq_bit = b->apu_frame_irq_pending ? 0x40u : 0x00u;
            u8 v = (u8)((b->open_bus & 0x20u) | irq_bit | status);
            b->apu_frame_irq_pending = false;
            b->open_bus = v;
            return v;
        }

        // $4016: controller port 1
        if (addr == 0x4016) {
            return read_controller_port(b, false);
        }

        // $4017: controller port 2
        if (addr == 0x4017) {
            return read_controller_port(b, true);
        }

        // $4000-$4013: basic APU reg readback placeholder from local state
        if (addr <= 0x4013u) {
            u8 v = b->apu_regs[(u8)(addr - 0x4000u)];
            b->open_bus = v;
            return v;
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
        // $4000-$4013: APU channel regs
        if (addr <= 0x4013u) {
            b->apu_regs[(u8)(addr - 0x4000u)] = data;
            apu_write_channel_len_reload(b, addr, data);
            return;
        }

        // $4014: OAMDMA
        if (addr == 0x4014) {
            begin_oam_dma(b, data);
            return;
        }

        // $4015: APU status/control
        if (addr == 0x4015) {
            b->apu_status = (u8)(data & 0x1Fu);

            if ((b->apu_status & 0x01u) == 0u) b->apu_length_ctr[0] = 0;
            if ((b->apu_status & 0x02u) == 0u) b->apu_length_ctr[1] = 0;
            if ((b->apu_status & 0x04u) == 0u) b->apu_length_ctr[2] = 0;
            if ((b->apu_status & 0x08u) == 0u) b->apu_length_ctr[3] = 0;
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
            b->apu_five_step_mode = (data & 0x80u) != 0;
            b->apu_frame_irq_inhibit = (data & 0x40u) != 0;
            if (b->apu_frame_irq_inhibit) {
                b->apu_frame_irq_pending = false;
            }
            b->apu_frame_cycle = 0;
            return;
        }

        return;
    }

    // $4020-$FFFF: cartridge space (mapper)
    if (b->cart) {
        (void)Cart_CPUWrite(b->cart, addr, data);
    }
}
