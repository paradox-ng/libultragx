#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ship/resource/File.h"

namespace Ship {

// Metadata an archive declares about itself (its manifest entry). A game reads this
// to show a mod's name/author. libultragx does not parse the manifest yet, so the
// base archive exposes an empty one.
struct ArchiveManifest {
    std::string Name;
    std::string Icon;
    std::string Author;
    std::string Version;
    std::string Website;
    std::string Description;
    std::string License;
    uint32_t CodeVersion = 0;
    uint32_t GameVersion = 0;
    std::string Main;
};

// Abstract archive: produces a File (raw bytes + a reader) for a named entry.
// The concrete implementation is O2rArchive (a ZIP-format .otr/.o2r).
class Archive {
  public:
    virtual ~Archive() = default;
    virtual bool HasFile(const std::string& filePath) = 0;
    virtual std::shared_ptr<File> LoadFile(const std::string& filePath) = 0;

    // All entry paths in this archive (used to build the CRC64 hash table).
    virtual const std::vector<std::string>& GetEntryNames() = 0;

    // On-disk path of this archive (e.g. "sd:/sm64.o2r"). Set by the concrete
    // archive when it opens; used for shader-pack manifest naming.
    const std::string& GetPath() const { return mPath; }

    // Archive manifest (mod name/author/etc.). Not parsed yet; returns an empty
    // manifest so a game's "show mod info" path compiles and reads blanks.
    virtual const ArchiveManifest& GetManifest() { return mManifest; }

  protected:
    std::string mPath;
    ArchiveManifest mManifest;
};

} // namespace Ship
