#include "nes/log.h"
#include "nes/nes.h"
#include "nes/frontend/sdl_app.h"

static void usage(const char* exe)
{
    NES_LOGI("Usage: %s path/to/rom.nes", exe);
    NES_LOGI("Controls: Arrows=Dpad, Z=B, X=A, RShift=Select, Enter=Start, Esc=Quit");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char* rom_path = argv[1];

    SdlApp app;
    if (!SdlApp_Init(&app, "NES Emulator (SDL3)", NES_FB_W, NES_FB_H, 3)) {
        return 1;
    }

    Nes nes;
    if (!NES_Init(&nes)) {
        SdlApp_Shutdown(&app);
        return 1;
    }

    if (!NES_LoadROM(&nes, rom_path)) {
        NES_Destroy(&nes);
        SdlApp_Shutdown(&app);
        return 1;
    }

    NES_Reset(&nes);

    while (!app.quit) {
        SdlApp_Poll(&app);

        // snapshot input into NES
        nes.input = app.input;

        // run one frame of emulation (placeholder now)
        NES_RunFrame(&nes);

        // present framebuffer
        SdlVideo_PresentARGB(&app.video, NES_Framebuffer(&nes), NES_FB_W, NES_FB_H);
    }

    NES_Destroy(&nes);
    SdlApp_Shutdown(&app);
    return 0;
}
