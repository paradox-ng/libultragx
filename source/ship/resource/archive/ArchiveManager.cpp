#include "ship/resource/archive/ArchiveManager.h"

namespace Ship {

void ArchiveManager::AddArchive(std::shared_ptr<Archive> archive) {
    if (archive != nullptr) {
        mArchives.push_back(std::move(archive));
    }
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

const char* ArchiveManager::HashToCString(uint64_t /*hash*/) const {
    return nullptr;
}

} // namespace Ship
