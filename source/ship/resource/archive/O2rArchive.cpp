#include "ship/resource/archive/O2rArchive.h"

#include "ship/resource/archive/zip.h"

namespace Ship {

O2rArchive::O2rArchive() : mZip(std::make_unique<ZipArchive>()) {
}

O2rArchive::~O2rArchive() = default;

bool O2rArchive::Open(const std::string& path) {
    mPath = path;
    return mZip->Open(path);
}

bool O2rArchive::HasFile(const std::string& filePath) {
    return mZip->Has(filePath);
}

std::shared_ptr<File> O2rArchive::LoadFile(const std::string& filePath) {
    if (!mZip->Has(filePath)) {
        return nullptr;
    }

    std::vector<uint8_t> bytes = mZip->Read(filePath);
    if (bytes.empty()) {
        return nullptr;
    }

    auto file = std::make_shared<File>();
    file->Buffer = std::make_shared<std::vector<char>>(
        reinterpret_cast<const char*>(bytes.data()),
        reinterpret_cast<const char*>(bytes.data()) + bytes.size());
    file->Reader = std::make_shared<BinaryReader>(file->Buffer->data(), file->Buffer->size());
    file->IsLoaded = true;
    // InitData is parsed by the ResourceManager (the header layout is not an
    // archive concern). Leave it null here.
    return file;
}

} // namespace Ship
