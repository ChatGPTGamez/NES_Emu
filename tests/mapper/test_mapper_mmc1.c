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

static void fill_chr_banks(Cart* c)
{
    const u32 bank_size = 4u * 1024u;
    u32 banks = c->chr_size / bank_size;
    for (u32 b = 0; b < banks; b++) {
        for (u32 i = 0; i < bank_size; i++) {
            c->chr[b * bank_size + i] = (u8)((b + 0x40u) & 0xFFu);
        }
    }
}

static void init_cart_for_mmc1(Cart* c)
{
    memset(c, 0, sizeof(*c));

    c->info.mapper = 1;
    c->prg_rom_size = 8u * 16u * 1024u; // 8 x 16KB banks
    c->prg_rom = (u8*)malloc((size_t)c->prg_rom_size);
    assert(c->prg_rom);
    fill_prg_banks(c);

    c->chr_size = 8u * 4u * 1024u; // 8 x 4KB banks
    c->chr = (u8*)malloc((size_t)c->chr_size);
    assert(c->chr);
    fill_chr_banks(c);
    c->chr_is_ram = true;

    c->prg_ram_size = 8u * 1024u;
    c->prg_ram = (u8*)calloc(1, (size_t)c->prg_ram_size);
    assert(c->prg_ram);

    c->mapper = Mapper_Create(c, 1);
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

static void mmc1_write_serial(Cart* c, u16 addr, u8 value)
{
    for (int i = 0; i < 5; i++) {
        u8 bit = (u8)((value >> i) & 1u);
        assert(Cart_CPUWrite(c, addr, bit));
    }
}

static void test_default_mode3_mapping(void)
{
    Cart c;
    init_cart_for_mmc1(&c);

    u8 v = 0;
    assert(Cart_CPURead(&c, 0x8000, &v));
    assert(v == 0); // switch bank defaults to 0

    assert(Cart_CPURead(&c, 0xC000, &v));
    assert(v == 7); // last bank fixed in mode 3

    free_cart(&c);
}

static void test_mode3_switch_low_bank(void)
{
    Cart c;
    init_cart_for_mmc1(&c);

    // PRG bank register = 3
    mmc1_write_serial(&c, 0xE000, 3u);

    u8 v = 0;
    assert(Cart_CPURead(&c, 0x8000, &v));
    assert(v == 3);
    assert(Cart_CPURead(&c, 0xC000, &v));
    assert(v == 7);

    free_cart(&c);
}

static void test_mode2_fixed_first_switch_high(void)
{
    Cart c;
    init_cart_for_mmc1(&c);

    // control: prg mode 2 (bit2=1, bit3=0)
    mmc1_write_serial(&c, 0x8000, 0x08u);
    mmc1_write_serial(&c, 0xE000, 4u);

    u8 v = 0;
    assert(Cart_CPURead(&c, 0x8000, &v));
    assert(v == 0);
    assert(Cart_CPURead(&c, 0xC000, &v));
    assert(v == 4);

    free_cart(&c);
}

static void test_mode0_32k_switching(void)
{
    Cart c;
    init_cart_for_mmc1(&c);

    // control mode 0 (32KB)
    mmc1_write_serial(&c, 0x8000, 0x00u);
    // prg bank 3 -> selects 32KB pair starting at bank2
    mmc1_write_serial(&c, 0xE000, 3u);

    u8 v = 0;
    assert(Cart_CPURead(&c, 0x8000, &v));
    assert(v == 2);
    assert(Cart_CPURead(&c, 0xC000, &v));
    assert(v == 3);

    free_cart(&c);
}

static void test_chr_mode1_two_4k_banks(void)
{
    Cart c;
    init_cart_for_mmc1(&c);

    // control: chr mode 1 + keep mode3 PRG
    mmc1_write_serial(&c, 0x8000, 0x1Cu);
    mmc1_write_serial(&c, 0xA000, 1u);
    mmc1_write_serial(&c, 0xC000, 2u);

    u8 v = 0;
    assert(Cart_PPURead(&c, 0x0000, &v));
    assert(v == (u8)(0x40u + 1u));

    assert(Cart_PPURead(&c, 0x1000, &v));
    assert(v == (u8)(0x40u + 2u));

    free_cart(&c);
}

static void test_prg_ram_roundtrip(void)
{
    Cart c;
    init_cart_for_mmc1(&c);

    assert(Cart_CPUWrite(&c, 0x6000, 0x5Au));

    u8 v = 0;
    assert(Cart_CPURead(&c, 0x6000, &v));
    assert(v == 0x5Au);

    free_cart(&c);
}

int main(void)
{
    test_default_mode3_mapping();
    test_mode3_switch_low_bank();
    test_mode2_fixed_first_switch_high();
    test_mode0_32k_switching();
    test_chr_mode1_two_4k_banks();
    test_prg_ram_roundtrip();
    puts("mapper mmc1: OK");
    return 0;
}
