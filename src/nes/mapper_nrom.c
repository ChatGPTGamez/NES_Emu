#include "nes/mapper.h"
#include "nes/cart.h"
#include <stdlib.h>

typedef struct MapperNROM {
    Mapper base;
} MapperNROM;

static bool nrom_cpu_read(Mapper* m, u16 addr, u8* out)
{
    Cart* c = m->cart;
    if (!c || !out) return false;

    // $6000-$7FFF : PRG RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (!c->prg_ram || c->prg_ram_size == 0) return false;
        u32 off = (u32)(addr - 0x6000);
        if (off >= c->prg_ram_size) off %= c->prg_ram_size;
        *out = c->prg_ram[off];
        return true;
    }

    // $8000-$FFFF : PRG ROM (NROM-128 16KB mirrored, NROM-256 32KB)
    if (addr >= 0x8000) {
        if (!c->prg_rom || c->prg_rom_size == 0) return false;
        u32 off = (u32)(addr - 0x8000);

        if (c->prg_rom_size == 16u * 1024u) {
            off &= 0x3FFFu; // mirror 16KB
        } else {
            off &= 0x7FFFu; // 32KB
            if (off >= c->prg_rom_size) off %= c->prg_rom_size;
        }

        *out = c->prg_rom[off];
        return true;
    }

    return false;
}

static bool nrom_cpu_write(Mapper* m, u16 addr, u8 data)
{
    Cart* c = m->cart;
    if (!c) return false;

    // $6000-$7FFF : PRG RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (!c->prg_ram || c->prg_ram_size == 0) return false;
        u32 off = (u32)(addr - 0x6000);
        if (off >= c->prg_ram_size) off %= c->prg_ram_size;
        c->prg_ram[off] = data;
        return true;
    }

    // Writes to $8000+ are ignored for NROM (no bank switching)
    if (addr >= 0x8000) {
        return true;
    }

    return false;
}

static bool nrom_ppu_read(Mapper* m, u16 addr, u8* out)
{
    Cart* c = m->cart;
    if (!c || !out) return false;

    // $0000-$1FFF : CHR ROM/RAM
    if (addr <= 0x1FFF) {
        if (!c->chr || c->chr_size == 0) return false;
        u32 off = (u32)addr;
        if (off >= c->chr_size) off %= c->chr_size;
        *out = c->chr[off];
        return true;
    }

    return false;
}

static bool nrom_ppu_write(Mapper* m, u16 addr, u8 data)
{
    Cart* c = m->cart;
    if (!c) return false;

    // Only writable if CHR is RAM
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

static void nrom_destroy(Mapper* m)
{
    (void)m;
}

Mapper* MapperNROM_Create(Cart* cart)
{
    MapperNROM* n = (MapperNROM*)calloc(1, sizeof(MapperNROM));
    if (!n) return NULL;

    n->base.id = 0;
    n->base.cart = cart;

    n->base.cpu_read  = nrom_cpu_read;
    n->base.cpu_write = nrom_cpu_write;
    n->base.ppu_read  = nrom_ppu_read;
    n->base.ppu_write = nrom_ppu_write;
    n->base.destroy   = nrom_destroy;

    return &n->base;
}
