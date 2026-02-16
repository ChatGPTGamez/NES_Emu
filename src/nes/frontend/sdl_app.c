#include "nes/frontend/sdl_app.h"
#include "nes/log.h"

static void set_key(NesInput* in, SDL_Keycode key, bool down)
{
    switch (key) {
        case SDLK_X:        NesInput_Set(in, NES_BTN_A, down); break;
        case SDLK_Z:        NesInput_Set(in, NES_BTN_B, down); break;
        case SDLK_RETURN:   NesInput_Set(in, NES_BTN_START, down); break;
        case SDLK_RSHIFT:   NesInput_Set(in, NES_BTN_SELECT, down); break;
        case SDLK_UP:       NesInput_Set(in, NES_BTN_UP, down); break;
        case SDLK_DOWN:     NesInput_Set(in, NES_BTN_DOWN, down); break;
        case SDLK_LEFT:     NesInput_Set(in, NES_BTN_LEFT, down); break;
        case SDLK_RIGHT:    NesInput_Set(in, NES_BTN_RIGHT, down); break;
        default: break;
    }
}

bool SdlApp_Init(SdlApp* app, const char* title, int fb_w, int fb_h, int scale)
{
    if (!app) return false;
    app->quit = false;
    app->input.p1 = 0;

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
                set_key(&app->input, e.key.key, true);
                break;

            case SDL_EVENT_KEY_UP:
                set_key(&app->input, e.key.key, false);
                break;

            default:
                break;
        }
    }
}
