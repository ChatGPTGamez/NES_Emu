#include "nes/ines.h"
#include <string.h>

static u32 u32_max(u32 a, u32 b) { return (a > b) ? a : b; }

bool INes_Parse(const u8* rom, size_t rom_size, INesInfo* out_info,
                size_t* out_prg_off, size_t* out_chr_off)
{
    if (!rom || rom_size < 16 || !out_info || !out_prg_off || !out_chr_off) return false;

    // Magic: 'N' 'E' 'S' 0x1A
    if (!(rom[0] == 'N' && rom[1] == 'E' && rom[2] == 'S' && rom[3] == 0x1A)) return false;

    INesInfo info;
    memset(&info, 0, sizeof(info));

    info.prg_rom_chunks = rom[4];
    info.chr_rom_chunks = rom[5];
    info.flags6 = rom[6];
    info.flags7 = rom[7];
    info.flags8 = rom[8];
    info.flags9 = rom[9];
    info.flags10 = rom[10];

    // NES 2.0 detection: bits 2..3 of flags7 == 0b10
    info.is_nes2 = ((info.flags7 & 0x0C) == 0x08);

    info.has_trainer = (info.flags6 & 0x04) != 0;
    info.has_battery = (info.flags6 & 0x02) != 0;

    bool four_screen = (info.flags6 & 0x08) != 0;
    bool vertical = (info.flags6 & 0x01) != 0;

    if (four_screen) info.mirroring = NES_MIRROR_FOURSCREEN;
    else info.mirroring = vertical ? NES_MIRROR_VERTICAL : NES_MIRROR_HORIZONTAL;

    if (!info.is_nes2) {
        // iNES 1.0 mapper: high nibble of flags6 + high nibble of flags7
        info.mapper = (u32)((info.flags6 >> 4) | (info.flags7 & 0xF0));

        info.prg_rom_size = (u32)info.prg_rom_chunks * 16u * 1024u;
        info.chr_rom_size = (u32)info.chr_rom_chunks * 8u * 1024u;

        // iNES 1.0 PRG RAM heuristic: if byte 8 is 0, treat as 8KB
        u32 prg_ram_units = info.flags8;
        info.prg_ram_size = (prg_ram_units == 0) ? (8u * 1024u) : (prg_ram_units * 8u * 1024u);
    } else {
        // NES 2.0 minimal support:
        // - PRG/CHR ROM sizes use byte4/5 as LSB and byte9 nibbles as MSB.
        // - Mapper extends with bits from byte8 high nibble.
        // - PRG RAM / PRG NVRAM sizes come from byte10 nibbles (2^n * 64 bytes), 0 means absent.

        u8 prg_msb = (u8)(info.flags9 & 0x0F);
        u8 chr_msb = (u8)((info.flags9 >> 4) & 0x0F);

        // If MSB nibble is 0xF, NES2 allows exponent/multiplier encoding. We don't support that yet.
        if (prg_msb == 0x0F || chr_msb == 0x0F) return false;

        u32 prg_units = (u32)info.prg_rom_chunks | ((u32)prg_msb << 8); // 16KB units
        u32 chr_units = (u32)info.chr_rom_chunks | ((u32)chr_msb << 8); // 8KB units

        info.prg_rom_size = prg_units * 16u * 1024u;
        info.chr_rom_size = chr_units * 8u * 1024u;

        // Mapper in NES2:
        // lower 8 bits: (flags6>>4) | (flags7&0xF0)
        // upper 4 bits: (byte8 & 0xF0)
        u32 mapper_low  = (u32)((info.flags6 >> 4) | (info.flags7 & 0xF0));
        u32 mapper_high = (u32)(info.flags8 & 0xF0);
        info.mapper = mapper_low | (mapper_high << 4);

        // PRG RAM sizes: byte10 low nibble = PRG-RAM, high nibble = PRG-NVRAM
        // size = 64 << n (i.e., 2^n * 64), with n==0 meaning 0 bytes.
        u8 prg_ram_shift   = (u8)(info.flags10 & 0x0F);
        u8 prg_nvram_shift = (u8)((info.flags10 >> 4) & 0x0F);

        u32 prg_ram  = (prg_ram_shift   == 0) ? 0u : (64u << prg_ram_shift);
        u32 prg_nram = (prg_nvram_shift == 0) ? 0u : (64u << prg_nvram_shift);

        // We'll allocate at least 8KB if any PRG RAM is expected/used by many games.
        // If both are 0, still allocate 8KB as a safe default for early bring-up.
        info.prg_ram_size = u32_max(8u * 1024u, prg_ram + prg_nram);
    }

    size_t off = 16;
    if (info.has_trainer) off += 512;

    size_t prg_off = off;
    size_t prg_end = prg_off + (size_t)info.prg_rom_size;
    if (prg_end > rom_size) return false;

    size_t chr_off = prg_end;
    size_t chr_end = chr_off + (size_t)info.chr_rom_size;
    if (chr_end > rom_size) return false;

    *out_info = info;
    *out_prg_off = prg_off;
    *out_chr_off = chr_off;
    return true;
}
