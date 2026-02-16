#pragma once
#include "nes/common.h"

typedef struct Cart Cart;

typedef struct Mapper {
    u32 id;
    Cart* cart;

    bool (*cpu_read)(struct Mapper* m, u16 addr, u8* out);
    bool (*cpu_write)(struct Mapper* m, u16 addr, u8 data);

    bool (*ppu_read)(struct Mapper* m, u16 addr, u8* out);
    bool (*ppu_write)(struct Mapper* m, u16 addr, u8 data);

    void (*destroy)(struct Mapper* m);
} Mapper;

Mapper* Mapper_Create(Cart* cart, u32 mapper_id);
void    Mapper_Destroy(Mapper* m);
