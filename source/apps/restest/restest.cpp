// libultragx resource-header test: parse real LUS resource headers from sm64.o2r.
//
// Validates the first stage of the resource pipeline end to end on real data:
// ZipArchive (decompress an entry) -> MemoryStream/BinaryReader -> ResourceInitData
// header. The header is: ByteOrder(u8, sets endianness for the rest), IsCustom(u8),
// 2 pad, Type(u32 FourCC), ResourceVersion(u32), Id(u64 = 0xDEADBEEFDEADBEEF).
// The archive is little-endian and we are big-endian, so this exercises the
// BinaryReader byte-swap path.
//
// Result as a GX clear color (frame dump can read it):
//   GREEN  = both headers parsed with the expected type + id
//   RED    = open failed     ORANGE = a header mismatched
// Press START (GC) / HOME (Wii) to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <vector>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "platform/sd.h"
#include "ship/resource/archive/zip.h"
#include "ship/utils/binarytools/BinaryReader.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)

// FourCCs as read from a little-endian uint32 (so "OTEX" bytes are X,E,T,O).
static const uint32_t TYPE_OTEX = 0x4F544558; // texture
static const uint32_t TYPE_ANIM = 0x414E494D; // animation
static const uint64_t RES_ID = 0xDEADBEEFDEADBEEFULL;

static void *frameBuffer[2] = { NULL, NULL };
static GXRModeObj *rmode = NULL;

struct Header {
    uint8_t byteOrder, isCustom;
    uint32_t type, version;
    uint64_t id;
};

static bool parse_header(std::vector<uint8_t> &data, Header &h) {
    if (data.size() < 20) return false;
    Ship::BinaryReader r((char *)data.data(), data.size());
    h.byteOrder = (uint8_t)r.ReadInt8();
    r.SetEndianness((Ship::Endianness)h.byteOrder);
    h.isCustom = (uint8_t)r.ReadInt8();
    r.ReadInt8();
    r.ReadInt8();
    h.type = r.ReadUInt32();
    h.version = r.ReadUInt32();
    h.id = r.ReadUInt64();
    return true;
}

// Run the test, write a log, return: 0 pass, 1 open fail, 2 header mismatch.
static int run_test(void) {
    std::string log;
    char line[256];

    const char *dev = lugx_sd_mount();
    snprintf(line, sizeof(line), "SD: %s\n", dev ? dev : "MOUNT FAILED");
    log += line;

    Ship::ZipArchive z;
    if (!z.Open("sd:/sm64.o2r")) {
        log += "open failed\n";
        if (dev) { FILE *f = fopen("sd:/restest.log", "w"); if (f) { fwrite(log.data(), 1, log.size(), f); fclose(f); } }
        return 1;
    }

    struct { const char *name; uint32_t expectType; } cases[] = {
        { "actors/amp/amp_body", TYPE_OTEX },
        { "actors/amp/anims/amp_animation", TYPE_ANIM },
    };

    int result = 0;
    for (auto &c : cases) {
        auto data = z.Read(c.name);
        Header h{};
        bool ok = parse_header(data, h);
        bool typeOk = ok && h.type == c.expectType;
        bool idOk = ok && h.id == RES_ID;
        char t[5] = { (char)(h.type & 0xff), (char)((h.type >> 8) & 0xff),
                      (char)((h.type >> 16) & 0xff), (char)((h.type >> 24) & 0xff), 0 };
        snprintf(line, sizeof(line),
                 "%-34s bytes=%u order=%u type='%s'(%08X) ver=%u idOk=%d -> %s\n",
                 c.name, (unsigned)data.size(), h.byteOrder, t, (unsigned)h.type, (unsigned)h.version,
                 idOk ? 1 : 0, (typeOk && idOk) ? "PASS" : "FAIL");
        log += line;
        if (!typeOk || !idOk) result = 2;
    }

    if (dev) { FILE *f = fopen("sd:/restest.log", "w"); if (f) { fwrite(log.data(), 1, log.size(), f); fclose(f); } }
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

    int result = run_test();
    GXColor colors[3] = {
        { 0x20, 0xE0, 0x20, 0xff }, // 0 pass   -> green
        { 0xE0, 0x20, 0x20, 0xff }, // 1 open   -> red
        { 0xF0, 0x80, 0x00, 0xff }, // 2 header -> orange
    };
    GXColor bg = colors[result % 3];

    void *gpfifo = memalign(32, DEFAULT_FIFO_SIZE);
    memset(gpfifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gpfifo, DEFAULT_FIFO_SIZE);
    GX_SetCopyClear(bg, GX_MAX_Z24);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, GX_SetDispCopyYScale(GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight)));
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
        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        GX_DrawDone();
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fb ^= 1;
    }

    lugx_sd_unmount();
    return 0;
}
