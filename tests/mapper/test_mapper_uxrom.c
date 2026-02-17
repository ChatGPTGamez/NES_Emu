#include "nes/mapper.h"
#include "nes/cart.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fill_prg_banks(Cart* c)
{
    const u32 bank_size = 16u * 1024u;
    u32 banks = c->prg_rom_size / bank_size;
    for (u32 b = 0; b < banks; b++) {
        for (u32 i = 0; i < bank_size; i++) {
            c->prg_rom[b * bank_size + i] = (u8)(b & 0xFFu);
        }
    }
}

static void init_cart_for_uxrom(Cart* c)
{
    memset(c, 0, sizeof(*c));

    c->info.mapper = 2;
    c->prg_rom_size = 4u * 16u * 1024u; // 4 banks
    c->prg_rom = (u8*)malloc((size_t)c->prg_rom_size);
    assert(c->prg_rom);
    fill_prg_banks(c);

    c->chr_size = 8u * 1024u;
    c->chr = (u8*)calloc(1, (size_t)c->chr_size);
    assert(c->chr);
    c->chr_is_ram = true;

    c->prg_ram_size = 8u * 1024u;
    c->prg_ram = (u8*)calloc(1, (size_t)c->prg_ram_size);
    assert(c->prg_ram);

    c->mapper = Mapper_Create(c, 2);
    assert(c->mapper);
}

static void free_cart(Cart* c)
{
    if (!c) return;
    if (c->mapper) Mapper_Destroy(c->mapper);
    free(c->prg_rom);
    free(c->chr);
    free(c->prg_ram);
    memset(c, 0, sizeof(*c));
}

static void test_switchable_low_bank_and_fixed_high_bank(void)
{
    Cart c;
    init_cart_for_uxrom(&c);

    u8 v = 0;

    // initial low bank should be 0
    assert(Cart_CPURead(&c, 0x8000, &v));
    assert(v == 0);

    // fixed high bank should be last bank (3)
    assert(Cart_CPURead(&c, 0xC000, &v));
    assert(v == 3);

    assert(Cart_CPUWrite(&c, 0x8000, 2));

    // low bank switched to bank 2
    assert(Cart_CPURead(&c, 0x8000, &v));
    assert(v == 2);

    // high bank remains fixed to last
    assert(Cart_CPURead(&c, 0xC000, &v));
    assert(v == 3);

    free_cart(&c);
}

static void test_prg_ram_roundtrip(void)
{
    Cart c;
    init_cart_for_uxrom(&c);

    assert(Cart_CPUWrite(&c, 0x6000, 0xA5));

    u8 v = 0;
    assert(Cart_CPURead(&c, 0x6000, &v));
    assert(v == 0xA5u);

    free_cart(&c);
}

static void test_chr_ram_roundtrip(void)
{
    Cart c;
    init_cart_for_uxrom(&c);

    assert(Cart_PPUWrite(&c, 0x0012, 0x5Au));

    u8 v = 0;
    assert(Cart_PPURead(&c, 0x0012, &v));
    assert(v == 0x5Au);

    free_cart(&c);
}

int main(void)
{
    test_switchable_low_bank_and_fixed_high_bank();
    test_prg_ram_roundtrip();
    test_chr_ram_roundtrip();
    puts("mapper uxrom: OK");
    return 0;
}
