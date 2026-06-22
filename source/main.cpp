// libultragx - M1 smoke test: SD mount + Ship::Context path resolution.
//
// Console diagnostics (no GX yet) validating, on real GameCube hardware
// (PicoBoot + Swiss + SD2SP2), that the SD mounts, argv[0] from the loader is
// usable, and the libultraship-compatible Ship::Context path API resolves the
// per-game folder. Press START (GC) or HOME (Wii) to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <dirent.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "platform/sd.h"
#include "ship/Context.h"
#include "libultraship/bridge.h"

static void *xfb = nullptr;
static GXRModeObj *rmode = nullptr;

static void list_dir(const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        printf("    (cannot open %s)\n", path);
        return;
    }
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue; // skip . / .. / dotfiles
        printf("    %s\n", e->d_name);
        if (++n >= 12) { printf("    ...\n"); break; }
    }
    if (n == 0) printf("    (empty)\n");
    closedir(d);
}

int main(int argc, char **argv) {
    VIDEO_Init();
    PAD_Init();
#ifdef HW_RVL
    WPAD_Init();
#endif

    rmode = VIDEO_GetPreferredMode(nullptr);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    printf("\x1b[2J"); // clear screen
    printf("libultragx - M1 smoke test\n");
    printf("==========================\n\n");

    // 1) What did the loader hand us?
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc && i < 4; i++)
        printf("  argv[%d] = %s\n", i, argv[i] ? argv[i] : "(null)");
    printf("\n");

    // 2) Mount the SD card.
    const char *dev = lugx_sd_mount();
    if (dev) printf("SD mounted via: %s\n", dev);
    else     printf("SD mount FAILED (no SD2SP2 / SD Gecko / Wii SD found)\n");
    printf("\n");

    // 3) Resolve paths through the libultraship-compatible Ship::Context API.
    Ship::Context::InitPaths(argc > 0 && argv[0] ? argv[0] : "", "libultragx");
    std::string appDir   = Ship::Context::GetAppDirectoryPath();
    std::string savePath = Ship::Context::GetPathRelativeToAppDirectory("save.bin");
    std::string located  = Ship::Context::LocateFileAcrossAppDirs("save.bin");

    printf("Ship::Context::GetAppDirectoryPath()           = %s\n", appDir.c_str());
    printf("Ship::Context::GetPathRelativeToAppDirectory() = %s\n", savePath.c_str());
    printf("Ship::Context::LocateFileAcrossAppDirs(save)   = %s\n",
           located.empty() ? "(not found)" : located.c_str());

    // 4) List the resolved app directory.
    if (dev) {
        printf("\ncontents of %s:\n", appDir.c_str());
        list_dir(appDir.c_str());
    }

    // 5) Exercise the CVar bridge (the C API Ghostship's glue calls ~9x).
    CVarSetInteger("gTestValue", 42);
    CVarRegisterInteger("gTestValue", 7); // no-op: already set
    CVarSetString("gTestName", "libultragx");
    printf("\nCVar gTestValue = %d (expect 42)\n", CVarGetInteger("gTestValue", -1));
    printf("CVar gTestName  = %s\n", CVarGetString("gTestName", "(unset)"));
    printf("CVar gMissing   = %d (expect -1)\n", CVarGetInteger("gMissing", -1));

    printf("\nPress START (GC) / HOME (Wii) to exit.\n");

    while (1) {
        PAD_ScanPads();
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) break;
#ifdef HW_RVL
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;
#endif
        VIDEO_WaitVSync();
    }

    lugx_sd_unmount();
    return 0;
}
