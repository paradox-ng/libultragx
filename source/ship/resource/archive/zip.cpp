#include "ship/resource/archive/zip.h"

#include <cstring>
#include <zlib.h>

namespace Ship {

// Little-endian field readers (ZIP is always little-endian, host may not be).
static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
}

// Raw deflate inflate (ZIP stores raw deflate with no zlib header: -MAX_WBITS).
static bool inflate_raw(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstLen) {
    z_stream s;
    memset(&s, 0, sizeof(s));
    if (inflateInit2(&s, -MAX_WBITS) != Z_OK) {
        return false;
    }
    s.next_in = (Bytef*)src;
    s.avail_in = (uInt)srcLen;
    s.next_out = (Bytef*)dst;
    s.avail_out = (uInt)dstLen;
    int r = inflate(&s, Z_FINISH);
    inflateEnd(&s);
    return r == Z_STREAM_END;
}

ZipArchive::~ZipArchive() {
    Close();
}

bool ZipArchive::Open(const std::string& path) {
    Close();
    // SPIKE (o2r streaming): keep the SD file handle open and stream every entry
    // read straight from it, instead of loading the whole ~10MB archive into RAM.
    // On a 24MB GameCube the resident archive was ~42% of RAM. The RAM-resident
    // design was chosen because interleaving the archive's SD reads with libfat
    // WRITES (debug log flushes) deadlocks Dolphin's emulated SD after a few dozen
    // ops; with logging off during play this is read-only, so streaming should be
    // safe. ReadCentralDirectory + Read already operate on mFile via fseek/fread,
    // so pointing mFile at the live SD handle needs no other change.
    mFile = fopen(path.c_str(), "rb");
    if (mFile == nullptr) {
        return false;
    }
    if (!ReadCentralDirectory()) {
        Close();
        return false;
    }
    return true;
}

void ZipArchive::Close() {
    if (mFile != nullptr) {
        fclose(mFile);
        mFile = nullptr;
    }
    mData.clear();
    mData.shrink_to_fit();
    mEntries.clear();
    mNames.clear();
}

bool ZipArchive::ReadCentralDirectory() {
    fseek(mFile, 0, SEEK_END);
    long fileSize = ftell(mFile);
    if (fileSize < 22) {
        return false;
    }

    // The End Of Central Directory record is within the last 64KB+22 bytes.
    long scan = (fileSize < 65557) ? fileSize : 65557;
    std::vector<uint8_t> tail(scan);
    fseek(mFile, fileSize - scan, SEEK_SET);
    if (fread(tail.data(), 1, scan, mFile) != (size_t)scan) {
        return false;
    }

    long eocd = -1;
    for (long i = scan - 22; i >= 0; i--) {
        if (rd32(&tail[i]) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        return false;
    }

    uint16_t totalEntries = rd16(&tail[eocd + 10]);
    uint32_t cdSize = rd32(&tail[eocd + 12]);
    uint32_t cdOffset = rd32(&tail[eocd + 16]);

    std::vector<uint8_t> cd(cdSize);
    fseek(mFile, cdOffset, SEEK_SET);
    if (fread(cd.data(), 1, cdSize, mFile) != cdSize) {
        return false;
    }

    mNames.reserve(totalEntries);
    size_t p = 0;
    for (uint16_t e = 0; e < totalEntries; e++) {
        if (p + 46 > cd.size() || rd32(&cd[p]) != 0x02014b50) {
            break;
        }
        Entry ent;
        ent.method = rd16(&cd[p + 10]);
        ent.compressedSize = rd32(&cd[p + 20]);
        ent.uncompressedSize = rd32(&cd[p + 24]);
        uint16_t nameLen = rd16(&cd[p + 28]);
        uint16_t extraLen = rd16(&cd[p + 30]);
        uint16_t commentLen = rd16(&cd[p + 32]);
        ent.localHeaderOffset = rd32(&cd[p + 42]);
        if (p + 46 + nameLen > cd.size()) {
            break;
        }
        std::string name((const char*)&cd[p + 46], nameLen);
        mEntries[name] = ent;
        mNames.push_back(std::move(name));
        p += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}

std::vector<uint8_t> ZipArchive::Read(const std::string& name) {
    auto it = mEntries.find(name);
    if (it == mEntries.end()) {
        return {};
    }
    const Entry& e = it->second;

    // The local header repeats name/extra lengths (the central-dir extra field
    // may differ from the local one), so read them from the local header.
    uint8_t lh[30];
    fseek(mFile, e.localHeaderOffset, SEEK_SET);
    if (fread(lh, 1, 30, mFile) != 30 || rd32(lh) != 0x04034b50) {
        return {};
    }
    uint16_t nameLen = rd16(&lh[26]);
    uint16_t extraLen = rd16(&lh[28]);
    long dataOff = (long)e.localHeaderOffset + 30 + nameLen + extraLen;

    std::vector<uint8_t> comp(e.compressedSize);
    fseek(mFile, dataOff, SEEK_SET);
    if (fread(comp.data(), 1, e.compressedSize, mFile) != e.compressedSize) {
        return {};
    }

    std::vector<uint8_t> out(e.uncompressedSize);
    if (e.method == 0) { // stored
        out = comp;
    } else if (e.method == 8) { // deflate
        if (!inflate_raw(comp.data(), comp.size(), out.data(), out.size())) {
            return {};
        }
    } else {
        return {};
    }
    return out;
}

} // namespace Ship
