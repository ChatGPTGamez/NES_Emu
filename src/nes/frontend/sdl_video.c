#include "nes/frontend/sdl_video.h"
#include "nes/log.h"

bool SdlVideo_Init(SdlVideo* v, const char* title, int w, int h, int scale)
{
    if (!v) return false;

    v->window = NULL;
    v->renderer = NULL;
    v->texture = NULL;
    v->tex_w = w;
    v->tex_h = h;

    int win_w = w * (scale > 0 ? scale : 3);
    int win_h = h * (scale > 0 ? scale : 3);

    v->window = SDL_CreateWindow(title, win_w, win_h, 0);
    if (!v->window) {
        NES_LOGE("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    // SDL3: no flags, no index
    v->renderer = SDL_CreateRenderer(v->window, NULL);
    if (!v->renderer) {
        NES_LOGE("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(v->window);
        v->window = NULL;
        return false;
    }

    v->texture = SDL_CreateTexture(
        v->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        w,
        h
    );

    if (!v->texture) {
        NES_LOGE("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(v->renderer);
        SDL_DestroyWindow(v->window);
        v->renderer = NULL;
        v->window = NULL;
        return false;
    }

    SDL_SetTextureScaleMode(v->texture, SDL_SCALEMODE_NEAREST);

    // SDL3 vsync is controlled via renderer properties, not flags
    SDL_SetRenderVSync(v->renderer, 1);

    return true;
}

void SdlVideo_Shutdown(SdlVideo* v)
{
    if (!v) return;

    if (v->texture)  SDL_DestroyTexture(v->texture);
    if (v->renderer) SDL_DestroyRenderer(v->renderer);
    if (v->window)   SDL_DestroyWindow(v->window);

    v->texture = NULL;
    v->renderer = NULL;
    v->window = NULL;
}

bool SdlVideo_PresentARGB(SdlVideo* v, const u32* argb_pixels, int w, int h)
{
    if (!v || !v->renderer || !v->texture) return false;
    if (!argb_pixels) return false;
    if (w != v->tex_w || h != v->tex_h) return false;

    const int pitch = w * (int)sizeof(u32);

    if (!SDL_UpdateTexture(v->texture, NULL, argb_pixels, pitch)) {
        NES_LOGE("SDL_UpdateTexture failed: %s", SDL_GetError());
        return false;
    }

    SDL_RenderClear(v->renderer);
    SDL_RenderTexture(v->renderer, v->texture, NULL, NULL);
    SDL_RenderPresent(v->renderer);

    return true;
}
