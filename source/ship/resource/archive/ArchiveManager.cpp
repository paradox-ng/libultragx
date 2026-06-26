#include "ship/resource/archive/ArchiveManager.h"

#include "ship/resource/archive/O2rArchive.h"
#include "ship/utils/StrHash64.h"

namespace Ship {

void ArchiveManager::AddArchive(std::shared_ptr<Archive> archive) {
    if (archive == nullptr) {
        return;
    }
    // Index every entry by its CRC64 path hash so OTR display lists (which
    // reference resources by hash) can be resolved back to a path.
    for (const auto& name : archive->GetEntryNames()) {
        mHashes[CRC64(name.c_str())] = name;
    }
    mArchives.push_back(std::move(archive));
}

std::shared_ptr<Archive> ArchiveManager::AddArchive(const std::string& archivePath) {
    auto archive = std::make_shared<O2rArchive>();
    if (!archive->Open(archivePath)) {
        return nullptr;
    }
    AddArchive(archive);
    return archive;
}

std::shared_ptr<std::vector<std::string>> ArchiveManager::ListFiles(const std::string& searchMask) {
    // Honor a single trailing '*' (the only form the game uses, e.g. "sound/banks/*").
    std::string prefix = searchMask;
    bool wildcard = false;
    auto star = searchMask.find('*');
    if (star != std::string::npos) {
        prefix = searchMask.substr(0, star);
        wildcard = true;
    }

    auto out = std::make_shared<std::vector<std::string>>();
    for (const auto& archive : mArchives) {
        if (archive == nullptr) {
            continue;
        }
        for (const auto& name : archive->GetEntryNames()) {
            bool match = wildcard ? (name.compare(0, prefix.size(), prefix) == 0)
                                  : (searchMask.empty() || name == searchMask);
            if (match) {
                out->push_back(name);
            }
        }
    }
    return out;
}

std::vector<uint32_t> ArchiveManager::GetGameVersions() {
    // The game version hash lives in the archive's "version" file: 1 byte endianness
    // marker (non-zero = big-endian) followed by the 32-bit hash in that endianness.
    // The game keys its geo function table on this hash; a 0 here leaves that table
    // empty, so every geo-asm function (camera update, Mario switches, the intro logo,
    // the title Mario head) resolves to null and never runs.
    auto file = LoadFile("version");
    if (file != nullptr && file->Buffer != nullptr && file->Buffer->size() >= 5) {
        const uint8_t* d = reinterpret_cast<const uint8_t*>(file->Buffer->data());
        const bool big = d[0] != 0;
        const uint32_t v = big ? ((uint32_t)d[1] << 24 | (uint32_t)d[2] << 16 | (uint32_t)d[3] << 8 | d[4])
                               : ((uint32_t)d[4] << 24 | (uint32_t)d[3] << 16 | (uint32_t)d[2] << 8 | d[1]);
        return { v };
    }
    return { 0 };
}

bool ArchiveManager::HasFile(const std::string& filePath) {
    for (auto it = mArchives.rbegin(); it != mArchives.rend(); ++it) {
        if (*it && (*it)->HasFile(filePath)) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<File> ArchiveManager::LoadFile(const std::string& filePath) {
    // Last-added archive wins, so a patch layered on top overrides the base.
    for (auto it = mArchives.rbegin(); it != mArchives.rend(); ++it) {
        if (*it && (*it)->HasFile(filePath)) {
            return (*it)->LoadFile(filePath);
        }
    }
    return nullptr;
}

std::shared_ptr<std::vector<std::shared_ptr<Archive>>> ArchiveManager::GetArchives() {
    return std::make_shared<std::vector<std::shared_ptr<Archive>>>(mArchives);
}

const char* ArchiveManager::HashToCString(uint64_t hash) const {
    auto it = mHashes.find(hash);
    return it != mHashes.end() ? it->second.c_str() : nullptr;
}

} // namespace Ship
