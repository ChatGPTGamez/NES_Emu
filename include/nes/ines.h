#pragma once
#include "nes/common.h"
#include <stddef.h>

typedef enum NesMirroring {
    NES_MIRROR_HORIZONTAL = 0,
    NES_MIRROR_VERTICAL   = 1,
    NES_MIRROR_FOURSCREEN = 2
} NesMirroring;

typedef struct INesInfo {
    u8 prg_rom_chunks;   // 16KB units
    u8 chr_rom_chunks;   // 8KB units
    u8 flags6;
    u8 flags7;
    u8 flags8;
    u8 flags9;
    u8 flags10;

    u32 mapper;          // iNES mapper number
    bool has_trainer;    // 512-byte trainer present
    bool has_battery;    // battery-backed PRG RAM
    bool is_nes2;        // NES 2.0 format detected (we reject it for now)
    NesMirroring mirroring;

    u32 prg_rom_size;    // bytes
    u32 chr_rom_size;    // bytes
    u32 prg_ram_size;    // bytes (iNES 1.0 heuristic)
} INesInfo;

// Parses an iNES 1.0 header and returns offsets/sizes for PRG/CHR data.
// Returns false if invalid or unsupported (e.g., NES 2.0).
bool INes_Parse(const u8* rom, size_t rom_size, INesInfo* out_info,
                size_t* out_prg_off, size_t* out_chr_off);
