// libultragx ResourceManager test (resource/gbi side, no GX).
//
// Exercises the full load path end to end: O2rArchive (zip storage) ->
// ArchiveManager -> ResourceManager.LoadResource(name) -> header parse ->
// DisplayListFactory (registered for the ODLT FourCC) -> typed Fast::DisplayList,
// plus a second load to confirm the cache returns the same instance.
//
// Separate TU from rmtest.cpp: this pulls fast/resource/type/DisplayList.h (gbi
// Gfx/Vtx), which cannot coexist with libogc gx.h.

#include <cstdio>
#include <memory>
#include <string>

#include "platform/sd.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/O2rArchive.h"
#include "fast/resource/factory/DisplayListFactory.h"
#include "fast/resource/type/DisplayList.h"

#define TYPE_ODLT 0x4F444C54u // DisplayList FourCC (little-endian uint32 'ODLT')

// Result codes (rmtest.cpp colors them): 0 pass, 1 archive open/mount, 2 load
// miss / wrong type, 3 decode or cache sanity fail.
int lugx_rmtest_run(void) {
    std::string log;
    char line[256];

    const char* dev = lugx_sd_mount();
    snprintf(line, sizeof(line), "SD: %s\n", dev ? dev : "MOUNT FAILED");
    log += line;

    auto writeLog = [&]() {
        if (dev) {
            FILE* f = fopen("sd:/rmtest.log", "w");
            if (f) { fwrite(log.data(), 1, log.size(), f); fclose(f); }
        }
    };

    auto archive = std::make_shared<Ship::O2rArchive>();
    if (!archive->Open("sd:/sm64.o2r")) {
        log += "archive open failed\n";
        writeLog();
        return 1;
    }

    Ship::ResourceManager rm;
    rm.GetArchiveManager()->AddArchive(archive);
    rm.RegisterResourceFactory(TYPE_ODLT, std::make_shared<Fast::DisplayListFactory>());
    snprintf(line, sizeof(line), "archives=%u\n", (unsigned)rm.GetArchiveManager()->GetArchiveCount());
    log += line;

    const char* name = "actors/amp/amp_electricity_dl";
    auto res = rm.LoadResource(name);
    if (res == nullptr) {
        log += "LoadResource returned null\n";
        writeLog();
        return 2;
    }

    auto dl = std::static_pointer_cast<Fast::DisplayList>(res);
    auto init = dl->GetInitData();
    size_t count = dl->Instructions.size();
    int8_t lastOp = count ? (int8_t)(dl->Instructions.back().words.w0 >> 24) : 0;
    bool typeOk = init && init->Type == TYPE_ODLT;
    bool terminated = count > 1 && count < 2000 &&
                      (lastOp == (int8_t)0xB8 || lastOp == (int8_t)0xDF);

    // Cache: a second load and GetCachedResource must both return this instance.
    bool cacheOk = rm.GetCachedResource(name) == res && rm.LoadResource(name) == res;

    snprintf(line, sizeof(line), "%s type=%08X ucode=%d cmds=%u lastOp=%02X cache=%d\n",
             name, (unsigned)(init ? init->Type : 0), (int)dl->UCode, (unsigned)count,
             (unsigned char)lastOp, (int)cacheOk);
    log += line;

    int result = (typeOk && terminated && cacheOk) ? 0 : 3;
    snprintf(line, sizeof(line), "-> %s\n", result == 0 ? "PASS" : "FAIL");
    log += line;
    writeLog();
    return result;
}
