// libultragx DisplayList decode test (resource/gbi side, no GX).
//
// Kept separate from dltest.cpp because this TU pulls in gbi.h (N64 Vtx) which
// collides with libogc gx.h (GX Vtx). Loads a real DisplayList from sm64.o2r and
// runs the real decoder over it; returns a result code the GX side colors.

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "platform/sd.h"
#include "ship/resource/archive/zip.h"
#include "ship/resource/File.h"
#include "ship/utils/binarytools/BinaryReader.h"
#include "fast/resource/DisplayListDecode.h"

#define TYPE_ODLT 0x4F444C54u // DisplayList FourCC (from a little-endian uint32)

int lugx_dltest_run(void) {
    std::string log;
    char line[256];

    const char *dev = lugx_sd_mount();
    snprintf(line, sizeof(line), "SD: %s\n", dev ? dev : "MOUNT FAILED");
    log += line;

    auto writeLog = [&]() {
        if (dev) { FILE *f = fopen("sd:/dltest.log", "w"); if (f) { fwrite(log.data(), 1, log.size(), f); fclose(f); } }
    };

    Ship::ZipArchive z;
    if (!z.Open("sd:/sm64.o2r")) {
        log += "open failed\n";
        writeLog();
        return 1;
    }

    const char *name = "actors/amp/amp_electricity_dl";
    auto data = z.Read(name);
    if (data.size() < 24) {
        log += "read failed / too small\n";
        writeLog();
        return 1;
    }

    // Parse the resource header, then hand the same reader (now at the payload)
    // to the decoder.
    auto reader = std::make_shared<Ship::BinaryReader>((char *)data.data(), data.size());
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->ByteOrder = (Ship::Endianness)reader->ReadInt8();
    reader->SetEndianness(init->ByteOrder);
    init->IsCustom = (bool)reader->ReadInt8();
    reader->ReadInt8();
    reader->ReadInt8();
    init->Type = reader->ReadUInt32();
    init->ResourceVersion = reader->ReadUInt32();
    init->Id = reader->ReadUInt64();
    init->Path = name;

    auto dl = lugx_read_display_list(init, reader);

    size_t count = dl->Instructions.size();
    int8_t lastOp = count ? (int8_t)(dl->Instructions.back().words.w0 >> 24) : 0;
    bool typeOk = init->Type == TYPE_ODLT;
    bool terminated = count > 1 && count < 2000 &&
                      (lastOp == (int8_t)0xB8 || lastOp == (int8_t)0xDF);

    snprintf(line, sizeof(line), "%s type=%08X ucode=%d cmds=%u lastOp=%02X -> %s\n",
             name, (unsigned)init->Type, (int)dl->UCode, (unsigned)count, (unsigned char)lastOp,
             (typeOk && terminated) ? "PASS" : "FAIL");
    log += line;
    writeLog();

    return (typeOk && terminated) ? 0 : 2;
}
