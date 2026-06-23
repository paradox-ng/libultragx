#pragma once

#include <memory>
#include <string>

#include "ship/resource/archive/Archive.h"

namespace Ship {
class ZipArchive;

// Concrete Archive backed by a ZIP-format .otr/.o2r on disk. The ZipArchive
// storage detail is held by pointer so this public header does not pull in the
// zip reader (which lives under source/).
class O2rArchive : public Archive {
  public:
    O2rArchive();
    ~O2rArchive() override;

    // Open the archive at `path` (e.g. "sd:/sm64.o2r"). Returns false if it
    // cannot be opened or has no central directory.
    bool Open(const std::string& path);

    bool HasFile(const std::string& filePath) override;
    std::shared_ptr<File> LoadFile(const std::string& filePath) override;
    const std::vector<std::string>& GetEntryNames() override;

  private:
    std::unique_ptr<ZipArchive> mZip;
};

} // namespace Ship
