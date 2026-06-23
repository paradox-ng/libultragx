#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ship/resource/File.h"

namespace Ship {

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

  protected:
    std::string mPath;
};

} // namespace Ship
