#pragma once
#include "nes/common.h"

typedef enum NesButton {
    NES_BTN_A      = 0,
    NES_BTN_B      = 1,
    NES_BTN_SELECT = 2,
    NES_BTN_START  = 3,
    NES_BTN_UP     = 4,
    NES_BTN_DOWN   = 5,
    NES_BTN_LEFT   = 6,
    NES_BTN_RIGHT  = 7
} NesButton;

typedef struct NesInput {
    u8 p1; // bit0=A, bit1=B, bit2=Select, bit3=Start, bit4=Up, bit5=Down, bit6=Left, bit7=Right
    u8 p2; // same bit layout as p1 for controller port 2
} NesInput;

static inline void NesInput_SetP1(NesInput* in, NesButton b, bool down)
{
    if (!in) return;
    u8 mask = (u8)(1u << (u8)b);
    if (down) in->p1 |= mask;
    else      in->p1 &= (u8)~mask;
}

static inline void NesInput_SetP2(NesInput* in, NesButton b, bool down)
{
    if (!in) return;
    u8 mask = (u8)(1u << (u8)b);
    if (down) in->p2 |= mask;
    else      in->p2 &= (u8)~mask;
}

static inline void NesInput_Set(NesInput* in, NesButton b, bool down)
{
    NesInput_SetP1(in, b, down);
}
