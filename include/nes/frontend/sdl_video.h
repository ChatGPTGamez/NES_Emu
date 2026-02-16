#pragma once
#include "nes/common.h"
#include <SDL3/SDL.h>

typedef struct SdlVideo {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int tex_w;
    int tex_h;
} SdlVideo;

bool SdlVideo_Init(SdlVideo* v, const char* title, int w, int h, int scale);
void SdlVideo_Shutdown(SdlVideo* v);

bool SdlVideo_PresentARGB(SdlVideo* v, const u32* argb_pixels, int w, int h);
