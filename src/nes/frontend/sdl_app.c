#include "nes/frontend/sdl_app.h"
#include "nes/log.h"

static void set_key_p1(NesInput* in, SDL_Keycode key, bool down)
{
    switch (key) {
        case SDLK_X:        NesInput_SetP1(in, NES_BTN_A, down); break;
        case SDLK_Z:        NesInput_SetP1(in, NES_BTN_B, down); break;
        case SDLK_RETURN:   NesInput_SetP1(in, NES_BTN_START, down); break;
        case SDLK_RSHIFT:   NesInput_SetP1(in, NES_BTN_SELECT, down); break;
        case SDLK_UP:       NesInput_SetP1(in, NES_BTN_UP, down); break;
        case SDLK_DOWN:     NesInput_SetP1(in, NES_BTN_DOWN, down); break;
        case SDLK_LEFT:     NesInput_SetP1(in, NES_BTN_LEFT, down); break;
        case SDLK_RIGHT:    NesInput_SetP1(in, NES_BTN_RIGHT, down); break;
        default: break;
    }
}

static void set_key_p2(NesInput* in, SDL_Keycode key, bool down)
{
    switch (key) {
        case SDLK_H:        NesInput_SetP2(in, NES_BTN_A, down); break;
        case SDLK_G:        NesInput_SetP2(in, NES_BTN_B, down); break;
        case SDLK_T:        NesInput_SetP2(in, NES_BTN_START, down); break;
        case SDLK_R:        NesInput_SetP2(in, NES_BTN_SELECT, down); break;
        case SDLK_W:        NesInput_SetP2(in, NES_BTN_UP, down); break;
        case SDLK_S:        NesInput_SetP2(in, NES_BTN_DOWN, down); break;
        case SDLK_A:        NesInput_SetP2(in, NES_BTN_LEFT, down); break;
        case SDLK_D:        NesInput_SetP2(in, NES_BTN_RIGHT, down); break;
        default: break;
    }
}

bool SdlApp_Init(SdlApp* app, const char* title, int fb_w, int fb_h, int scale)
{
    if (!app) return false;
    app->quit = false;
    app->input.p1 = 0;
    app->input.p2 = 0;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
        NES_LOGE("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    if (!SdlVideo_Init(&app->video, title, fb_w, fb_h, scale)) {
        SDL_Quit();
        return false;
    }

    return true;
}

void SdlApp_Shutdown(SdlApp* app)
{
    if (!app) return;
    SdlVideo_Shutdown(&app->video);
    SDL_Quit();
}

void SdlApp_Poll(SdlApp* app)
{
    if (!app) return;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                app->quit = true;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_ESCAPE) app->quit = true;
                set_key_p1(&app->input, e.key.key, true);
                set_key_p2(&app->input, e.key.key, true);
                break;

            case SDL_EVENT_KEY_UP:
                set_key_p1(&app->input, e.key.key, false);
                set_key_p2(&app->input, e.key.key, false);
                break;

            default:
                break;
        }
    }
}
