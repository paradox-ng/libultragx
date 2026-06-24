#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ship {

// Lean reader for a ZIP-format archive (.otr / .o2r) on disk - the storage layer
// under the resource manager. The central directory is parsed up front; entry
// bytes are decompressed on demand (stored or deflate, via zlib). ZIP integers
// are little-endian and are assembled byte-wise, so this is correct on the
// big-endian PowerPC target.
class ZipArchive {
  public:
    struct Entry {
        uint32_t localHeaderOffset = 0;
        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;
        uint16_t method = 0; // 0 = stored, 8 = deflate
    };

    ZipArchive() = default;
    ~ZipArchive();

    bool Open(const std::string& path);
    void Close();

    size_t Count() const { return mEntries.size(); }
    bool Has(const std::string& name) const { return mEntries.count(name) != 0; }
    const std::vector<std::string>& Names() const { return mNames; }

    // Decompressed bytes for the named entry, or empty on failure.
    std::vector<uint8_t> Read(const std::string& name);

  private:
    bool ReadCentralDirectory();

    FILE* mFile = nullptr;
    std::vector<uint8_t> mData; // the whole archive in RAM; mFile is an fmemopen view of it
    std::unordered_map<std::string, Entry> mEntries;
    std::vector<std::string> mNames;
};

} // namespace Ship
