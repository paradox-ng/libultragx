#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ship/resource/archive/Archive.h"

namespace Ship {

// Holds the set of mounted archives and resolves a file name against them. Later
// archives take precedence (added last = searched first), so a game can layer a
// patch .o2r over the base one.
class ArchiveManager {
  public:
    void AddArchive(std::shared_ptr<Archive> archive);
    size_t GetArchiveCount() const { return mArchives.size(); }

    bool HasFile(const std::string& filePath);
    std::shared_ptr<File> LoadFile(const std::string& filePath);

  private:
    std::vector<std::shared_ptr<Archive>> mArchives;
};

} // namespace Ship
