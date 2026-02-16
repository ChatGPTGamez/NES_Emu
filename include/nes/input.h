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
} NesInput;

static inline void NesInput_Set(NesInput* in, NesButton b, bool down)
{
    if (!in) return;
    u8 mask = (u8)(1u << (u8)b);
    if (down) in->p1 |= mask;
    else      in->p1 &= (u8)~mask;
}
