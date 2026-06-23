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

    // libultraship-compatible surface used by the Fast3D interpreter.
    std::shared_ptr<std::vector<std::shared_ptr<Archive>>> GetArchives();
    // Resolve a CRC64 resource hash (carried by OTR-expanded display-list opcodes)
    // back to its archive path. Stubbed (returns nullptr) until the CRC64 name
    // table is built; the hash-addressed draw path is not exercised before then.
    const char* HashToCString(uint64_t hash) const;

  private:
    std::vector<std::shared_ptr<Archive>> mArchives;
};

} // namespace Ship
