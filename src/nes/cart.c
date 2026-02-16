#include "nes/cart.h"
#include "nes/log.h"
#include "nes/util/file.h"
#include "nes/mapper.h"
#include "nes/ines.h"
#include <string.h>
#include <stdlib.h>

static void cart_free_all(Cart* c)
{
    if (!c) return;

    if (c->mapper) { Mapper_Destroy(c->mapper); c->mapper = NULL; }

    if (c->prg_rom) { File_Free(c->prg_rom); c->prg_rom = NULL; }
    if (c->chr)     { File_Free(c->chr);     c->chr = NULL; }
    if (c->prg_ram) { File_Free(c->prg_ram); c->prg_ram = NULL; }

    c->prg_rom_size = 0;
    c->chr_size = 0;
    c->prg_ram_size = 0;
    c->chr_is_ram = false;
    memset(&c->info, 0, sizeof(c->info));
}

bool Cart_Init(Cart* c)
{
    if (!c) return false;
    memset(c, 0, sizeof(*c));
    return true;
}

void Cart_Destroy(Cart* c)
{
    if (!c) return;
    cart_free_all(c);
}

bool Cart_LoadFromFile(Cart* c, const char* path)
{
    if (!c || !path) return false;

    cart_free_all(c);

    unsigned char* rom = NULL;
    size_t rom_size = 0;
    if (!File_ReadAllBytes(path, &rom, &rom_size)) {
        NES_LOGE("Cart: failed to read ROM: %s", path);
        return false;
    }

    INesInfo info;
    size_t prg_off = 0;
    size_t chr_off = 0;

    if (!INes_Parse((const u8*)rom, rom_size, &info, &prg_off, &chr_off)) {
        NES_LOGE("Cart: invalid or unsupported NES ROM header: %s", path);
        File_Free(rom);
        return false;
    }

    // PRG ROM
    c->prg_rom_size = info.prg_rom_size;
    c->prg_rom = (u8*)malloc((size_t)c->prg_rom_size);
    if (!c->prg_rom) { File_Free(rom); return false; }
    memcpy(c->prg_rom, rom + prg_off, (size_t)c->prg_rom_size);

    // CHR ROM or CHR RAM
    if (info.chr_rom_size > 0) {
        c->chr_size = info.chr_rom_size;
        c->chr = (u8*)malloc((size_t)c->chr_size);
        if (!c->chr) { File_Free(rom); cart_free_all(c); return false; }
        memcpy(c->chr, rom + chr_off, (size_t)c->chr_size);
        c->chr_is_ram = false;
    } else {
        c->chr_size = 8u * 1024u;
        c->chr = (u8*)malloc((size_t)c->chr_size);
        if (!c->chr) { File_Free(rom); cart_free_all(c); return false; }
        memset(c->chr, 0, (size_t)c->chr_size);
        c->chr_is_ram = true;
    }

    // PRG RAM
    c->prg_ram_size = info.prg_ram_size;
    if (c->prg_ram_size == 0) c->prg_ram_size = 8u * 1024u; // safe default
    c->prg_ram = (u8*)malloc((size_t)c->prg_ram_size);
    if (!c->prg_ram) { File_Free(rom); cart_free_all(c); return false; }
    memset(c->prg_ram, 0, (size_t)c->prg_ram_size);

    c->info = info;

    File_Free(rom);

    // Create mapper
    c->mapper = Mapper_Create(c, c->info.mapper);
    if (!c->mapper) {
        NES_LOGE("Cart: failed to create mapper %u", c->info.mapper);
        cart_free_all(c);
        return false;
    }

    NES_LOGI("Cart: mapper=%u, PRG=%u, CHR=%u (%s), mirroring=%d",
             c->info.mapper,
             c->prg_rom_size,
             c->chr_size,
             c->chr_is_ram ? "RAM" : "ROM",
             (int)c->info.mirroring);

    return true;
}

bool Cart_CPURead(Cart* c, u16 addr, u8* out)
{
    if (!c || !c->mapper || !c->mapper->cpu_read) return false;
    return c->mapper->cpu_read(c->mapper, addr, out);
}

bool Cart_CPUWrite(Cart* c, u16 addr, u8 data)
{
    if (!c || !c->mapper || !c->mapper->cpu_write) return false;
    return c->mapper->cpu_write(c->mapper, addr, data);
}

bool Cart_PPURead(Cart* c, u16 addr, u8* out)
{
    if (!c || !c->mapper || !c->mapper->ppu_read) return false;
    return c->mapper->ppu_read(c->mapper, addr, out);
}

bool Cart_PPUWrite(Cart* c, u16 addr, u8 data)
{
    if (!c || !c->mapper || !c->mapper->ppu_write) return false;
    return c->mapper->ppu_write(c->mapper, addr, data);
}
