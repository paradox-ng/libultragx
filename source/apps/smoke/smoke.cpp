// libultragx - smoke test: SD mount + Ship::Context path resolution.
//
// Console diagnostics (no GX yet) validating, on real GameCube hardware
// (PicoBoot + Swiss + SD2SP2), that the SD mounts, argv[0] from the loader is
// usable, and the libultraship-compatible Ship::Context path API resolves the
// per-game folder. Everything printed is also teed to "libultragx.log" on the SD
// so it can be read off the card afterwards. Press START (GC) or HOME (Wii) to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include <string>

#include "platform/sd.h"
#include "ship/Context.h"
#include "libultraship/bridge.h"
#include "ship/utils/binarytools/BinaryReader.h"
#include "ship/utils/binarytools/BinaryWriter.h"
#include "ship/utils/binarytools/MemoryStream.h"

static void *xfb = nullptr;
static GXRModeObj *rmode = nullptr;

// Accumulates everything reported so it can be written to the SD log file.
static std::string gReport;

// Print a line to both the console and the in-memory log buffer.
static void report(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
    gReport += buf;
}

static void list_dir(const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        report("    (cannot open %s)\n", path);
        return;
    }
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue; // skip . / .. / dotfiles
        report("    %s\n", e->d_name);
        if (++n >= 12) { report("    ...\n"); break; }
    }
    if (n == 0) report("    (empty)\n");
    closedir(d);
}

// Write the accumulated report to <appDir>libultragx.log, falling back to the
// SD root. Returns the path written, or "" on failure.
static std::string write_log(const std::string &appDir) {
    std::string path = appDir + "libultragx.log";
    FILE *f = fopen(path.c_str(), "w");
    if (!f) {
        path = "sd:/libultragx.log";
        f = fopen(path.c_str(), "w");
    }
    if (!f) return "";
    fwrite(gReport.data(), 1, gReport.size(), f);
    fclose(f);
    return path;
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

    printf("\x1b[2J"); // clear screen (console-only, not logged)
    report("libultragx - smoke test\n");
    report("=======================\n\n");

    // 1) What did the loader hand us?
    report("argc = %d\n", argc);
    for (int i = 0; i < argc && i < 4; i++)
        report("  argv[%d] = %s\n", i, argv[i] ? argv[i] : "(null)");
    report("\n");

    // 2) Mount the SD card.
    const char *dev = lugx_sd_mount();
    if (dev) report("SD mounted via: %s\n", dev);
    else     report("SD mount FAILED (no SD2SP2 / SD Gecko / Wii SD found)\n");
    report("\n");

    // 3) Resolve paths through the libultraship-compatible Ship::Context API.
    Ship::Context::InitPaths(argc > 0 && argv[0] ? argv[0] : "", "libultragx");
    std::string appDir   = Ship::Context::GetAppDirectoryPath();
    std::string savePath = Ship::Context::GetPathRelativeToAppDirectory("save.bin");
    std::string located  = Ship::Context::LocateFileAcrossAppDirs("save.bin");

    report("Ship::Context::GetAppDirectoryPath()           = %s\n", appDir.c_str());
    report("Ship::Context::GetPathRelativeToAppDirectory() = %s\n", savePath.c_str());
    report("Ship::Context::LocateFileAcrossAppDirs(save)   = %s\n",
           located.empty() ? "(not found)" : located.c_str());

    // 4) List the resolved app directory.
    if (dev) {
        report("\ncontents of %s:\n", appDir.c_str());
        list_dir(appDir.c_str());
    }

    // 5) Exercise the CVar bridge (the C API Ghostship's glue calls ~9x).
    CVarSetInteger("gTestValue", 42);
    CVarRegisterInteger("gTestValue", 7); // no-op: already set
    CVarSetString("gTestName", "libultragx");
    report("\nCVar gTestValue = %d (expect 42)\n", CVarGetInteger("gTestValue", -1));
    report("CVar gTestName  = %s\n", CVarGetString("gTestName", "(unset)"));
    report("CVar gMissing   = %d (expect -1)\n", CVarGetInteger("gMissing", -1));

    // 7) Binary IO + endianness round-trip (the big-endian PPC concern, exercised).
    {
        auto stream = std::make_shared<Ship::MemoryStream>();
        Ship::BinaryWriter w(stream);
        w.SetEndianness(Ship::Endianness::Big);
        w.Write((uint32_t)0x11223344);

        std::vector<char> raw = stream->ToVector();
        report("\nBinaryWriter: %u bytes, first = 0x%02X (expect 0x11, big-endian)\n",
               (unsigned)raw.size(), raw.empty() ? 0 : (unsigned char)raw[0]);

        Ship::BinaryReader r(stream);
        r.Seek(0, Ship::SeekOffsetType::Start);
        r.SetEndianness(Ship::Endianness::Big);
        uint32_t asBig = r.ReadUInt32();
        r.Seek(0, Ship::SeekOffsetType::Start);
        r.SetEndianness(Ship::Endianness::Little);
        uint32_t asLittle = r.ReadUInt32();
        report("BinaryReader Big    = 0x%08X (expect 0x11223344)\n", (unsigned)asBig);
        report("BinaryReader Little = 0x%08X (expect 0x44332211)\n", (unsigned)asLittle);
    }

    // 6) Persist the report to the SD so it can be read off the card.
    if (dev) {
        std::string logPath = write_log(appDir);
        // console-only (the buffer is already flushed to the file by now)
        if (!logPath.empty()) printf("\nlog written: %s\n", logPath.c_str());
        else                  printf("\n(could not write log file)\n");
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
