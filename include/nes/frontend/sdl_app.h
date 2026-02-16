#pragma once
#include "nes/common.h"
#include "nes/input.h"
#include "nes/frontend/sdl_video.h"
#include <SDL3/SDL.h>

typedef struct SdlApp {
    bool quit;
    SdlVideo video;
    NesInput input;
} SdlApp;

bool SdlApp_Init(SdlApp* app, const char* title, int fb_w, int fb_h, int scale);
void SdlApp_Shutdown(SdlApp* app);

// Poll SDL events and update app->quit and app->input
void SdlApp_Poll(SdlApp* app);
