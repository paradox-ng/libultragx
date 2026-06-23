#include "ship/resource/archive/ArchiveManager.h"

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
