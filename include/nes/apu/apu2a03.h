#pragma once
#include "nes/common.h"
#include <stdbool.h>

enum {
    APU_PULSE_CH1 = 0,
    APU_PULSE_CH2 = 1,
    APU_TRIANGLE  = 2,
    APU_NOISE     = 3,
    APU_DMC       = 4
};

typedef struct APU2A03 {
    // Raw register mirror for $4000-$4013
    u8 regs[0x14];

    // $4015 / $4017 control state
    u8 status_enable;
    u8 frame_counter;
    bool frame_irq_pending;
    bool frame_irq_inhibit;
    bool five_step_mode;
    u32 frame_cycle;

    // Channel length counters: pulse1, pulse2, triangle, noise
    u8 length_ctr[4];

    // Pulse channel state (minimal functional model)
    u16 pulse_timer_period[2];
    u16 pulse_timer_value[2];
    u8 pulse_seq_step[2];
    u8 pulse_duty[2];

    bool pulse_const_vol[2];
    bool pulse_env_loop[2];
    u8 pulse_env_period[2];
    u8 pulse_env_divider[2];
    u8 pulse_env_decay[2];
    bool pulse_env_start[2];

    u8 pulse_output[2];
} APU2A03;

bool APU2A03_Init(APU2A03* a);
void APU2A03_Reset(APU2A03* a);

void APU2A03_Write(APU2A03* a, u16 addr, u8 data);
u8 APU2A03_ReadStatus(APU2A03* a, u8 open_bus);

// Tick one CPU cycle. Returns true when frame IRQ should be requested.
bool APU2A03_Tick(APU2A03* a);
