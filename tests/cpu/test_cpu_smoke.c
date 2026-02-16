#include "nes/cpu/cpu6502.h"
#include "nes/bus.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static u8 g_mem[65536];

u8 Bus_CPURead(Bus* b, u16 addr)
{
    (void)b;
    return g_mem[addr];
}

void Bus_CPUWrite(Bus* b, u16 addr, u8 data)
{
    (void)b;
    g_mem[addr] = data;
}

static CPU6502 make_cpu(void)
{
    Bus bus;
    memset(&bus, 0, sizeof(bus));

    CPU6502 cpu;
    assert(CPU6502_Init(&cpu, &bus));
    return cpu;
}

static void test_brk_vectors_and_stack(void)
{
    memset(g_mem, 0, sizeof(g_mem));

    // Reset vector -> $8000
    g_mem[0xFFFC] = 0x00;
    g_mem[0xFFFD] = 0x80;

    // IRQ/BRK vector -> $1234
    g_mem[0xFFFE] = 0x34;
    g_mem[0xFFFF] = 0x12;

    g_mem[0x8000] = 0x00; // BRK
    g_mem[0x8001] = 0xEA; // BRK padding byte

    CPU6502 cpu = make_cpu();
    CPU6502_Reset(&cpu);

    assert(cpu.pc == 0x8000);
    assert(cpu.sp == 0xFD);

    int cyc = CPU6502_Step(&cpu);
    assert(cyc == 7);

    // BRK should vector via IRQ vector and push PC+2 + status.
    assert(cpu.pc == 0x1234);
    assert(cpu.sp == 0xFA);
    assert((cpu.p & F_I) != 0);
    assert(!cpu.jammed);

    assert(g_mem[0x01FD] == 0x80); // return hi ($8002)
    assert(g_mem[0x01FC] == 0x02); // return lo
    assert((g_mem[0x01FB] & F_B) != 0);
    assert((g_mem[0x01FB] & F_U) != 0);
}

static void test_nmi_pending_is_serviced_before_opcode(void)
{
    memset(g_mem, 0, sizeof(g_mem));

    // Reset vector -> $8000
    g_mem[0xFFFC] = 0x00;
    g_mem[0xFFFD] = 0x80;

    // NMI vector -> $5678
    g_mem[0xFFFA] = 0x78;
    g_mem[0xFFFB] = 0x56;

    g_mem[0x8000] = 0xEA; // NOP (must not execute in this step)

    CPU6502 cpu = make_cpu();
    CPU6502_Reset(&cpu);

    CPU6502_RequestNMI(&cpu);
    int cyc = CPU6502_Step(&cpu);

    assert(cyc == 7);
    assert(cpu.pc == 0x5678);
    assert(cpu.sp == 0xFA);
    assert((cpu.p & F_I) != 0);
}

static void test_irq_masking_and_service_behavior(void)
{
    memset(g_mem, 0, sizeof(g_mem));

    // Reset vector -> $8000
    g_mem[0xFFFC] = 0x00;
    g_mem[0xFFFD] = 0x80;

    // IRQ vector -> $2468
    g_mem[0xFFFE] = 0x68;
    g_mem[0xFFFF] = 0x24;

    // One harmless opcode at reset target.
    g_mem[0x8000] = 0xEA; // NOP

    CPU6502 cpu = make_cpu();
    CPU6502_Reset(&cpu);

    // While I flag is set after reset, pending IRQ must remain pending and not fire.
    assert((cpu.p & F_I) != 0);
    CPU6502_RequestIRQ(&cpu);

    int cyc = CPU6502_Step(&cpu);
    assert(cyc == 2);           // NOP cycles
    assert(cpu.pc == 0x8001);   // NOP executed
    assert(cpu.irq_pending);    // still pending because I masked IRQ

    // Unmask IRQ and ensure pending IRQ is serviced on the next step.
    cpu.p &= (u8)~F_I;
    cyc = CPU6502_Step(&cpu);

    assert(cyc == 7);
    assert(cpu.pc == 0x2468);
    assert(cpu.sp == 0xFA);
    assert((cpu.p & F_I) != 0);
    assert(!cpu.irq_pending);

    // IRQ push should have B clear and U set.
    assert(g_mem[0x01FD] == 0x80); // return hi ($8001)
    assert(g_mem[0x01FC] == 0x01); // return lo
    assert((g_mem[0x01FB] & F_B) == 0);
    assert((g_mem[0x01FB] & F_U) != 0);
}

int main(void)
{
    test_brk_vectors_and_stack();
    test_nmi_pending_is_serviced_before_opcode();
    test_irq_masking_and_service_behavior();
    puts("cpu smoke: OK");
    return 0;
}
