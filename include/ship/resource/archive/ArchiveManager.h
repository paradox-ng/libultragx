#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ship/resource/archive/Archive.h"
#include "ship/security/Keystore.h"

namespace Ship {

// Holds the set of mounted archives and resolves a file name against them. Later
// archives take precedence (added last = searched first), so a game can layer a
// patch .o2r over the base one.
class ArchiveManager {
  public:
    void AddArchive(std::shared_ptr<Archive> archive);
    // Open and mount the .o2r/.otr at `archivePath`. Returns the archive, or nullptr
    // if it could not be opened.
    std::shared_ptr<Archive> AddArchive(const std::string& archivePath);
    size_t GetArchiveCount() const { return mArchives.size(); }

    bool HasFile(const std::string& filePath);
    std::shared_ptr<File> LoadFile(const std::string& filePath);

    // Entry paths across all mounted archives matching a glob-ish mask (only a
    // single trailing '*' wildcard is honored, e.g. "sound/banks/*").
    std::shared_ptr<std::vector<std::string>> ListFiles(const std::string& searchMask = "");

    // Numeric game versions declared by the mounted archives. libultragx does not
    // parse archive version files yet, so this returns a single 0 entry (callers
    // index [0]).
    std::vector<uint32_t> GetGameVersions();

    // A game registers a callback to prompt about untrusted archives. libultragx
    // does not verify archive signatures, so the handler is stored but never run.
    void SetUntrustedArchiveHandler(std::function<void(Archive&, KeystoreEntry&)> handler) {
        mUntrustedArchiveHandler = std::move(handler);
    }

    // libultraship-compatible surface used by the Fast3D interpreter.
    std::shared_ptr<std::vector<std::shared_ptr<Archive>>> GetArchives();
    // Resolve a CRC64 resource hash (carried by OTR-expanded display-list opcodes)
    // back to its archive path. Stubbed (returns nullptr) until the CRC64 name
    // table is built; the hash-addressed draw path is not exercised before then.
    const char* HashToCString(uint64_t hash) const;

  private:
    std::vector<std::shared_ptr<Archive>> mArchives;
    std::unordered_map<uint64_t, std::string> mHashes; // CRC64(path) -> path
    std::function<void(Archive&, KeystoreEntry&)> mUntrustedArchiveHandler;
};

} // namespace Ship
