#include "nes/apu/apu2a03.h"
#include <string.h>

static const u8 k_apu_len_table[32] = {
    10, 254, 20, 2, 40, 4, 80, 6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

static const u8 k_duty_table[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1}
};

static void pulse_reload_from_reg3(APU2A03* a, int ch, u8 data)
{
    if ((a->status_enable & (u8)(1u << ch)) == 0u) return;

    a->length_ctr[ch] = k_apu_len_table[(data >> 3) & 0x1Fu];
    a->pulse_seq_step[ch] = 0;
    a->pulse_env_start[ch] = true;
}

static void pulse_clock_envelope(APU2A03* a, int ch)
{
    if (a->pulse_env_start[ch]) {
        a->pulse_env_start[ch] = false;
        a->pulse_env_decay[ch] = 15u;
        a->pulse_env_divider[ch] = a->pulse_env_period[ch];
        return;
    }

    if (a->pulse_env_divider[ch] > 0u) {
        a->pulse_env_divider[ch]--;
        return;
    }

    a->pulse_env_divider[ch] = a->pulse_env_period[ch];
    if (a->pulse_env_decay[ch] > 0u) {
        a->pulse_env_decay[ch]--;
    } else if (a->pulse_env_loop[ch]) {
        a->pulse_env_decay[ch] = 15u;
    }
}

static void clock_quarter_frame(APU2A03* a)
{
    pulse_clock_envelope(a, APU_PULSE_CH1);
    pulse_clock_envelope(a, APU_PULSE_CH2);
}

static void clock_half_frame(APU2A03* a)
{
    for (int ch = 0; ch < 4; ch++) {
        bool halt = false;
        if (ch == APU_PULSE_CH1) halt = a->pulse_env_loop[APU_PULSE_CH1];
        if (ch == APU_PULSE_CH2) halt = a->pulse_env_loop[APU_PULSE_CH2];

        if ((a->status_enable & (u8)(1u << ch)) && a->length_ctr[ch] > 0u && !halt) {
            a->length_ctr[ch]--;
        }
    }
}

static void pulse_tick_timer(APU2A03* a, int ch)
{
    if (a->pulse_timer_value[ch] == 0u) {
        a->pulse_timer_value[ch] = a->pulse_timer_period[ch];
        a->pulse_seq_step[ch] = (u8)((a->pulse_seq_step[ch] + 1u) & 7u);
    } else {
        a->pulse_timer_value[ch]--;
    }

    if ((a->status_enable & (u8)(1u << ch)) == 0u || a->length_ctr[ch] == 0u) {
        a->pulse_output[ch] = 0u;
        return;
    }

    if (a->pulse_timer_period[ch] < 8u) {
        a->pulse_output[ch] = 0u;
        return;
    }

    u8 duty_bit = k_duty_table[a->pulse_duty[ch] & 3u][a->pulse_seq_step[ch] & 7u];
    if (!duty_bit) {
        a->pulse_output[ch] = 0u;
        return;
    }

    u8 vol = a->pulse_const_vol[ch] ? (a->pulse_env_period[ch] & 0x0Fu)
                                    : (a->pulse_env_decay[ch] & 0x0Fu);
    a->pulse_output[ch] = vol;
}

bool APU2A03_Init(APU2A03* a)
{
    if (!a) return false;
    memset(a, 0, sizeof(*a));
    return true;
}

void APU2A03_Reset(APU2A03* a)
{
    if (!a) return;
    memset(a, 0, sizeof(*a));
}

void APU2A03_Write(APU2A03* a, u16 addr, u8 data)
{
    if (!a) return;

    if (addr >= 0x4000u && addr <= 0x4013u) {
        a->regs[(u8)(addr - 0x4000u)] = data;

        if (addr == 0x4000u) {
            a->pulse_duty[APU_PULSE_CH1] = (u8)((data >> 6) & 0x03u);
            a->pulse_env_loop[APU_PULSE_CH1] = (data & 0x20u) != 0;
            a->pulse_const_vol[APU_PULSE_CH1] = (data & 0x10u) != 0;
            a->pulse_env_period[APU_PULSE_CH1] = (u8)(data & 0x0Fu);
            return;
        }

        if (addr == 0x4002u) {
            a->pulse_timer_period[APU_PULSE_CH1] =
                (u16)((a->pulse_timer_period[APU_PULSE_CH1] & 0x0700u) | data);
            return;
        }

        if (addr == 0x4003u) {
            a->pulse_timer_period[APU_PULSE_CH1] =
                (u16)((a->pulse_timer_period[APU_PULSE_CH1] & 0x00FFu) |
                      (((u16)(data & 0x07u)) << 8));
            pulse_reload_from_reg3(a, APU_PULSE_CH1, data);
            return;
        }

        if (addr == 0x4004u) {
            a->pulse_duty[APU_PULSE_CH2] = (u8)((data >> 6) & 0x03u);
            a->pulse_env_loop[APU_PULSE_CH2] = (data & 0x20u) != 0;
            a->pulse_const_vol[APU_PULSE_CH2] = (data & 0x10u) != 0;
            a->pulse_env_period[APU_PULSE_CH2] = (u8)(data & 0x0Fu);
            return;
        }

        if (addr == 0x4006u) {
            a->pulse_timer_period[APU_PULSE_CH2] =
                (u16)((a->pulse_timer_period[APU_PULSE_CH2] & 0x0700u) | data);
            return;
        }

        if (addr == 0x4007u) {
            a->pulse_timer_period[APU_PULSE_CH2] =
                (u16)((a->pulse_timer_period[APU_PULSE_CH2] & 0x00FFu) |
                      (((u16)(data & 0x07u)) << 8));
            pulse_reload_from_reg3(a, APU_PULSE_CH2, data);
            return;
        }

        if (addr == 0x400Bu) {
            if (a->status_enable & 0x04u) {
                a->length_ctr[APU_TRIANGLE] = k_apu_len_table[(data >> 3) & 0x1Fu];
            }
            return;
        }

        if (addr == 0x400Fu) {
            if (a->status_enable & 0x08u) {
                a->length_ctr[APU_NOISE] = k_apu_len_table[(data >> 3) & 0x1Fu];
            }
            return;
        }

        return;
    }

    if (addr == 0x4015u) {
        a->status_enable = (u8)(data & 0x1Fu);

        if ((a->status_enable & 0x01u) == 0u) a->length_ctr[APU_PULSE_CH1] = 0u;
        if ((a->status_enable & 0x02u) == 0u) a->length_ctr[APU_PULSE_CH2] = 0u;
        if ((a->status_enable & 0x04u) == 0u) a->length_ctr[APU_TRIANGLE] = 0u;
        if ((a->status_enable & 0x08u) == 0u) a->length_ctr[APU_NOISE] = 0u;
        return;
    }

    if (addr == 0x4017u) {
        a->frame_counter = data;
        a->five_step_mode = (data & 0x80u) != 0;
        a->frame_irq_inhibit = (data & 0x40u) != 0;
        if (a->frame_irq_inhibit) {
            a->frame_irq_pending = false;
        }
        a->frame_cycle = 0u;
        return;
    }
}

u8 APU2A03_ReadStatus(APU2A03* a, u8 open_bus)
{
    if (!a) return open_bus;

    u8 status = 0u;
    if (a->length_ctr[APU_PULSE_CH1] > 0u) status |= 0x01u;
    if (a->length_ctr[APU_PULSE_CH2] > 0u) status |= 0x02u;
    if (a->length_ctr[APU_TRIANGLE] > 0u) status |= 0x04u;
    if (a->length_ctr[APU_NOISE] > 0u) status |= 0x08u;
    if (a->status_enable & 0x10u) status |= 0x10u; // DMC active approximation

    u8 irq_bit = a->frame_irq_pending ? 0x40u : 0x00u;
    u8 out = (u8)((open_bus & 0x20u) | irq_bit | status);

    a->frame_irq_pending = false;
    return out;
}

bool APU2A03_Tick(APU2A03* a)
{
    if (!a) return false;

    a->frame_cycle++;

    if (!a->five_step_mode) {
        if (a->frame_cycle == 3729u || a->frame_cycle == 11186u) {
            clock_quarter_frame(a);
        }
        if (a->frame_cycle == 7457u || a->frame_cycle == 14915u) {
            clock_quarter_frame(a);
            clock_half_frame(a);
        }

        if (a->frame_cycle >= 14915u) {
            if (!a->frame_irq_inhibit) {
                a->frame_irq_pending = true;
            }
            a->frame_cycle = 0u;
        }
    } else {
        if (a->frame_cycle == 3729u || a->frame_cycle == 11186u || a->frame_cycle == 18641u) {
            clock_quarter_frame(a);
        }
        if (a->frame_cycle == 7457u || a->frame_cycle == 14915u) {
            clock_quarter_frame(a);
            clock_half_frame(a);
        }

        if (a->frame_cycle >= 18641u) {
            a->frame_cycle = 0u;
        }
    }

    pulse_tick_timer(a, APU_PULSE_CH1);
    pulse_tick_timer(a, APU_PULSE_CH2);

    return a->frame_irq_pending && !a->frame_irq_inhibit;
}
