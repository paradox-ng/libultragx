// libultragx archive test: validate the ZIP archive reader against a real .o2r.
//
// Mounts the SD, opens sd:/sm64.o2r, parses it, and reports the result two ways:
//  - to the console + sd:/archtest.log (full detail: counts, group breakdown,
//    the version/portVersion bytes), and
//  - as a full-screen GX clear color, so a Dolphin frame dump can read the
//    outcome even though the rest is a VI console (which frame dump can't see):
//      GREEN  = pass (opened, expected entry count, metadata extracted)
//      RED    = open failed     ORANGE = wrong entry count     YELLOW = extract failed
//      (no frame at all = crashed during the read)
// Press START (GC) / HOME (Wii) to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <map>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "platform/sd.h"
#include "ship/resource/archive/zip.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)
#define EXPECTED_ENTRIES 17441

static void *frameBuffer[2] = { NULL, NULL };
static GXRModeObj *rmode = NULL;

// Run the archive test and write a detailed log; return a result code:
// 0 = pass, 1 = open failed, 2 = wrong count, 3 = extract failed.
static int run_test(void) {
    std::string log;
    auto logf = [&](const char *s) { log += s; };
    char line[256];

    const char *dev = lugx_sd_mount();
    snprintf(line, sizeof(line), "SD: %s\n", dev ? dev : "MOUNT FAILED");
    logf(line);

    int result = 0;
    Ship::ZipArchive z;
    if (!z.Open("sd:/sm64.o2r")) {
        logf("FAILED to open sd:/sm64.o2r\n");
        result = 1;
    } else {
        size_t count = z.Count();
        snprintf(line, sizeof(line), "opened sd:/sm64.o2r; entries: %u (expect %d)\n",
                 (unsigned)count, EXPECTED_ENTRIES);
        logf(line);

        std::map<std::string, int> groups;
        for (const std::string &n : z.Names()) {
            size_t slash = n.find('/');
            groups[slash == std::string::npos ? n : n.substr(0, slash)]++;
        }
        logf("top-level groups:\n");
        for (auto &g : groups) {
            snprintf(line, sizeof(line), "  %-12s %d\n", g.first.c_str(), g.second);
            logf(line);
        }

        bool extracted = true;
        for (const char *meta : { "version", "portVersion" }) {
            auto data = z.Read(meta);
            if (data.empty()) extracted = false;
            snprintf(line, sizeof(line), "%s: %u bytes\n", meta, (unsigned)data.size());
            logf(line);
        }

        if (count != EXPECTED_ENTRIES) result = 2;
        else if (!extracted) result = 3;
    }

    snprintf(line, sizeof(line), "result code: %d (0=pass)\n", result);
    logf(line);

    if (dev) {
        FILE *f = fopen("sd:/archtest.log", "w");
        if (f) { fwrite(log.data(), 1, log.size(), f); fclose(f); }
    }
    return result;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    VIDEO_Init();
    PAD_Init();
#ifdef HW_RVL
    WPAD_Init();
#endif

    rmode = VIDEO_GetPreferredMode(NULL);
    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    // Run the archive test (this also writes sd:/archtest.log).
    int result = run_test();

    GXColor colors[4] = {
        { 0x20, 0xE0, 0x20, 0xff }, // 0 pass   -> green
        { 0xE0, 0x20, 0x20, 0xff }, // 1 open   -> red
        { 0xF0, 0x80, 0x00, 0xff }, // 2 count  -> orange
        { 0xF0, 0xF0, 0x00, 0xff }, // 3 extract-> yellow
    };
    GXColor bg = colors[result & 3];

    void *gpfifo = memalign(32, DEFAULT_FIFO_SIZE);
    memset(gpfifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gpfifo, DEFAULT_FIFO_SIZE);
    GX_SetCopyClear(bg, GX_MAX_Z24);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, GX_SetDispCopyYScale(yscale));
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    u32 fb = 0;
    while (1) {
        PAD_ScanPads();
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) break;
#ifdef HW_RVL
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;
#endif
        GX_CopyDisp(frameBuffer[fb], GX_TRUE); // clears EFB to bg, copies to XFB
        GX_DrawDone();
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fb ^= 1;
    }

    lugx_sd_unmount();
    return 0;
}
