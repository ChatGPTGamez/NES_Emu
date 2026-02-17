#include "nes/mapper.h"
#include "nes/cart.h"
#include <stdlib.h>

typedef struct MapperUxROM {
    Mapper base;
    u8 bank_select;
} MapperUxROM;

static inline u32 prg_bank_count_16k(const Cart* c)
{
    if (!c || c->prg_rom_size == 0) return 0;
    return c->prg_rom_size / (16u * 1024u);
}

static bool uxrom_cpu_read(Mapper* m, u16 addr, u8* out)
{
    MapperUxROM* u = (MapperUxROM*)m;
    Cart* c = m->cart;
    if (!u || !c || !out) return false;

    // $6000-$7FFF: PRG RAM (common compatibility behavior)
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (!c->prg_ram || c->prg_ram_size == 0) return false;
        u32 off = (u32)(addr - 0x6000);
        if (off >= c->prg_ram_size) off %= c->prg_ram_size;
        *out = c->prg_ram[off];
        return true;
    }

    if (addr < 0x8000 || !c->prg_rom || c->prg_rom_size == 0) return false;

    u32 banks = prg_bank_count_16k(c);
    if (banks == 0) return false;

    u32 bank;
    u32 bank_off = (u32)(addr & 0x3FFFu);

    if (addr <= 0xBFFF) {
        // $8000-$BFFF: switchable 16KB bank
        bank = (u32)(u->bank_select % banks);
    } else {
        // $C000-$FFFF: fixed to last 16KB bank
        bank = banks - 1u;
    }

    u32 off = bank * (16u * 1024u) + bank_off;
    if (off >= c->prg_rom_size) off %= c->prg_rom_size;

    *out = c->prg_rom[off];
    return true;
}

static bool uxrom_cpu_write(Mapper* m, u16 addr, u8 data)
{
    MapperUxROM* u = (MapperUxROM*)m;
    Cart* c = m->cart;
    if (!u || !c) return false;

    // $6000-$7FFF: PRG RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (!c->prg_ram || c->prg_ram_size == 0) return false;
        u32 off = (u32)(addr - 0x6000);
        if (off >= c->prg_ram_size) off %= c->prg_ram_size;
        c->prg_ram[off] = data;
        return true;
    }

    if (addr >= 0x8000) {
        // UxROM: bank select from low bits.
        u->bank_select = data;
        return true;
    }

    return false;
}

static bool uxrom_ppu_read(Mapper* m, u16 addr, u8* out)
{
    Cart* c = m->cart;
    if (!c || !out) return false;

    // CHR is fixed (often CHR RAM on UxROM boards)
    if (addr <= 0x1FFF) {
        if (!c->chr || c->chr_size == 0) return false;
        u32 off = (u32)addr;
        if (off >= c->chr_size) off %= c->chr_size;
        *out = c->chr[off];
        return true;
    }

    return false;
}

static bool uxrom_ppu_write(Mapper* m, u16 addr, u8 data)
{
    Cart* c = m->cart;
    if (!c) return false;

    if (addr <= 0x1FFF) {
        if (!c->chr || c->chr_size == 0) return false;
        if (!c->chr_is_ram) return false;

        u32 off = (u32)addr;
        if (off >= c->chr_size) off %= c->chr_size;
        c->chr[off] = data;
        return true;
    }

    return false;
}

static void uxrom_destroy(Mapper* m)
{
    (void)m;
}

Mapper* MapperUxROM_Create(Cart* cart)
{
    MapperUxROM* u = (MapperUxROM*)calloc(1, sizeof(MapperUxROM));
    if (!u) return NULL;

    u->base.id = 2;
    u->base.cart = cart;

    u->base.cpu_read  = uxrom_cpu_read;
    u->base.cpu_write = uxrom_cpu_write;
    u->base.ppu_read  = uxrom_ppu_read;
    u->base.ppu_write = uxrom_ppu_write;
    u->base.destroy   = uxrom_destroy;

    u->bank_select = 0;

    return &u->base;
}
