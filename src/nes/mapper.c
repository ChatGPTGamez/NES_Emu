#include "nes/mapper.h"
#include "nes/cart.h"
#include "nes/log.h"
#include <stdlib.h>

Mapper* MapperNROM_Create(Cart* cart); // implemented in mapper_nrom.c

Mapper* Mapper_Create(Cart* cart, u32 mapper_id)
{
    if (!cart) return NULL;

    switch (mapper_id) {
        case 0: return MapperNROM_Create(cart);
        default:
            NES_LOGE("Mapper_Create: unsupported mapper %u", mapper_id);
            return NULL;
    }
}

void Mapper_Destroy(Mapper* m)
{
    if (!m) return;
    if (m->destroy) m->destroy(m);
    free(m);
}
