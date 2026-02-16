#pragma once
#include "nes/common.h"
#include "nes/ines.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct Mapper Mapper;

typedef struct Cart {
    INesInfo info;

    u8* prg_rom;
    u32 prg_rom_size;

    u8* chr;         // CHR ROM or CHR RAM buffer
    u32 chr_size;
    bool chr_is_ram;

    u8* prg_ram;
    u32 prg_ram_size;

    Mapper* mapper;
} Cart;

bool Cart_Init(Cart* c);
void Cart_Destroy(Cart* c);

bool Cart_LoadFromFile(Cart* c, const char* path);

// Mapper-facing accessors (what the bus will call later)
bool Cart_CPURead(Cart* c, u16 addr, u8* out);
bool Cart_CPUWrite(Cart* c, u16 addr, u8 data);

bool Cart_PPURead(Cart* c, u16 addr, u8* out);
bool Cart_PPUWrite(Cart* c, u16 addr, u8 data);
