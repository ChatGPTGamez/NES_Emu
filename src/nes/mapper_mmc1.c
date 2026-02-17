#include "nes/mapper.h"
#include "nes/cart.h"
#include <stdlib.h>

typedef struct MapperMMC1 {
    Mapper base;

    u8 shift;      // serial shift register (bit4 set when empty)
    u8 control;    // control register
    u8 chr_bank0;  // CHR bank 0 register
    u8 chr_bank1;  // CHR bank 1 register
    u8 prg_bank;   // PRG bank register
} MapperMMC1;

static inline u32 prg_bank_count_16k(const Cart* c)
{
    if (!c || c->prg_rom_size == 0) return 0;
    return c->prg_rom_size / (16u * 1024u);
}

static inline u32 chr_bank_count_4k(const Cart* c)
{
    if (!c || c->chr_size == 0) return 0;
    return c->chr_size / (4u * 1024u);
}

static inline bool prg_ram_disabled(const MapperMMC1* m)
{
    // Common MMC1 behavior: bit4 of PRG register disables PRG RAM when set.
    return (m->prg_bank & 0x10u) != 0;
}

static u8 mmc1_read_prg(MapperMMC1* m, u16 addr)
{
    Cart* c = m->base.cart;

    u32 banks16 = prg_bank_count_16k(c);
    if (banks16 == 0) return 0;

    u8 prg_mode = (u8)((m->control >> 2) & 0x03u);
    u32 bank16;
    u32 off16 = (u32)(addr & 0x3FFFu);

    if (prg_mode <= 1) {
        // 32KB mode: ignore low bit, map a pair of 16KB banks.
        u32 bank32 = (u32)((m->prg_bank & 0x0Eu) >> 1);
        u32 base16 = (bank32 * 2u) % banks16;
        bank16 = base16 + ((addr >= 0xC000) ? 1u : 0u);
        bank16 %= banks16;
    } else if (prg_mode == 2u) {
        // Fix first bank at $8000, switch bank at $C000.
        bank16 = (addr < 0xC000) ? 0u : (u32)(m->prg_bank & 0x0Fu);
        bank16 %= banks16;
    } else {
        // prg_mode == 3
        // Switch bank at $8000, fix last bank at $C000.
        bank16 = (addr < 0xC000) ? (u32)(m->prg_bank & 0x0Fu) : (banks16 - 1u);
        bank16 %= banks16;
    }

    u32 off = bank16 * (16u * 1024u) + off16;
    if (off >= c->prg_rom_size) off %= c->prg_rom_size;
    return c->prg_rom[off];
}

static u8 mmc1_read_chr(MapperMMC1* m, u16 addr)
{
    Cart* c = m->base.cart;

    u32 banks4 = chr_bank_count_4k(c);
    if (banks4 == 0 || !c->chr) return 0;

    u8 chr_mode = (u8)((m->control >> 4) & 0x01u);

    u32 bank4;
    u32 off4 = (u32)(addr & 0x0FFFu);

    if (chr_mode == 0u) {
        // 8KB mode: ignore low bit of chr_bank0.
        u32 base4 = (u32)(m->chr_bank0 & 0x1Eu);
        bank4 = base4 + ((addr >= 0x1000) ? 1u : 0u);
    } else {
        // 4KB mode: separate banks for low/high 4KB.
        bank4 = (addr < 0x1000) ? (u32)m->chr_bank0 : (u32)m->chr_bank1;
    }

    bank4 %= banks4;

    u32 off = bank4 * (4u * 1024u) + off4;
    if (off >= c->chr_size) off %= c->chr_size;
    return c->chr[off];
}

static void mmc1_write_reg(MapperMMC1* m, u16 addr, u8 val)
{
    if (addr <= 0x9FFF) {
        m->control = (u8)(val & 0x1Fu);
        return;
    }
    if (addr <= 0xBFFF) {
        m->chr_bank0 = (u8)(val & 0x1Fu);
        return;
    }
    if (addr <= 0xDFFF) {
        m->chr_bank1 = (u8)(val & 0x1Fu);
        return;
    }

    m->prg_bank = (u8)(val & 0x1Fu);
}

static bool mmc1_cpu_read(Mapper* base, u16 addr, u8* out)
{
    MapperMMC1* m = (MapperMMC1*)base;
    Cart* c = base->cart;
    if (!m || !c || !out) return false;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (prg_ram_disabled(m) || !c->prg_ram || c->prg_ram_size == 0) return false;
        u32 off = (u32)(addr - 0x6000);
        if (off >= c->prg_ram_size) off %= c->prg_ram_size;
        *out = c->prg_ram[off];
        return true;
    }

    if (addr >= 0x8000) {
        *out = mmc1_read_prg(m, addr);
        return true;
    }

    return false;
}

static bool mmc1_cpu_write(Mapper* base, u16 addr, u8 data)
{
    MapperMMC1* m = (MapperMMC1*)base;
    Cart* c = base->cart;
    if (!m || !c) return false;

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (prg_ram_disabled(m) || !c->prg_ram || c->prg_ram_size == 0) return false;
        u32 off = (u32)(addr - 0x6000);
        if (off >= c->prg_ram_size) off %= c->prg_ram_size;
        c->prg_ram[off] = data;
        return true;
    }

    if (addr < 0x8000) return false;

    if (data & 0x80u) {
        // Reset shift register and force control PRG mode 3 behavior.
        m->shift = 0x10u;
        m->control = (u8)(m->control | 0x0Cu);
        return true;
    }

    bool complete = (m->shift & 1u) != 0;
    m->shift = (u8)((m->shift >> 1) | ((data & 1u) ? 0x10u : 0x00u));

    if (complete) {
        mmc1_write_reg(m, addr, m->shift);
        m->shift = 0x10u;
    }

    return true;
}

static bool mmc1_ppu_read(Mapper* base, u16 addr, u8* out)
{
    MapperMMC1* m = (MapperMMC1*)base;
    if (!m || !out) return false;

    if (addr <= 0x1FFF) {
        *out = mmc1_read_chr(m, addr);
        return true;
    }

    return false;
}

static bool mmc1_ppu_write(Mapper* base, u16 addr, u8 data)
{
    MapperMMC1* m = (MapperMMC1*)base;
    Cart* c = base->cart;
    if (!m || !c) return false;

    if (addr > 0x1FFF || !c->chr || c->chr_size == 0 || !c->chr_is_ram) return false;

    u32 banks4 = chr_bank_count_4k(c);
    if (banks4 == 0) return false;

    u8 chr_mode = (u8)((m->control >> 4) & 0x01u);
    u32 bank4;
    u32 off4 = (u32)(addr & 0x0FFFu);

    if (chr_mode == 0u) {
        u32 base4 = (u32)(m->chr_bank0 & 0x1Eu);
        bank4 = base4 + ((addr >= 0x1000) ? 1u : 0u);
    } else {
        bank4 = (addr < 0x1000) ? (u32)m->chr_bank0 : (u32)m->chr_bank1;
    }

    bank4 %= banks4;

    u32 off = bank4 * (4u * 1024u) + off4;
    if (off >= c->chr_size) off %= c->chr_size;
    c->chr[off] = data;
    return true;
}

static void mmc1_destroy(Mapper* m)
{
    (void)m;
}

Mapper* MapperMMC1_Create(Cart* cart)
{
    MapperMMC1* m = (MapperMMC1*)calloc(1, sizeof(MapperMMC1));
    if (!m) return NULL;

    m->base.id = 1;
    m->base.cart = cart;

    m->base.cpu_read  = mmc1_cpu_read;
    m->base.cpu_write = mmc1_cpu_write;
    m->base.ppu_read  = mmc1_ppu_read;
    m->base.ppu_write = mmc1_ppu_write;
    m->base.destroy   = mmc1_destroy;

    m->shift = 0x10u;
    m->control = 0x0Cu;
    m->chr_bank0 = 0;
    m->chr_bank1 = 0;
    m->prg_bank = 0;

    return &m->base;
}
