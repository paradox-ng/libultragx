// libultragx - M1 smoke test: SD mount + per-game path resolution.
//
// Console diagnostics (no GX yet) so the SD2SP2 + per-game-folder layout can be
// validated on real GameCube hardware (PicoBoot + Swiss). It reports what the
// loader passed as argv, which SD device mounted, the base directory resolved
// from argv[0], and the contents of that directory. The M0 spinning triangle is
// preserved in git history. Press START (GC) or HOME (Wii) to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "platform/sd.h"
#include "platform/paths.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static void list_dir(const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        printf("    (cannot open %s)\n", path);
        return;
    }
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue; // skip . and .. and dotfiles
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

    rmode = VIDEO_GetPreferredMode(NULL);
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

    // 1) What did the loader hand us? (Tells us how Swiss passes the .dol path.)
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc && i < 4; i++)
        printf("  argv[%d] = %s\n", i, argv[i] ? argv[i] : "(null)");
    printf("\n");

    // 2) Mount the SD card.
    const char *dev = lugx_sd_mount();
    if (dev) printf("SD mounted via: %s\n", dev);
    else     printf("SD mount FAILED (no SD2SP2 / SD Gecko / Wii SD found)\n");
    printf("\n");

    // 3) Resolve the per-game base directory from argv[0].
    char base[256];
    lugx_resolve_base_dir(argc > 0 ? argv[0] : NULL, base, sizeof base);
    printf("resolved base dir: %s\n", base);

    // 4) Does it exist? List what's in it (assets, saves, ...).
    if (dev) {
        printf("contents:\n");
        list_dir(base);
    }

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
