#include "ship/resource/ResourceManager.h"

namespace Ship {

ResourceManager::ResourceManager() : mArchiveManager(std::make_shared<ArchiveManager>()) {
}

void ResourceManager::RegisterResourceFactory(uint32_t type, std::shared_ptr<ResourceFactory> factory) {
    if (factory != nullptr) {
        mFactories[type] = std::move(factory);
    }
}

std::shared_ptr<IResource> ResourceManager::GetCachedResource(const std::string& filePath) {
    auto it = mCache.find(filePath);
    return it != mCache.end() ? it->second : nullptr;
}

std::shared_ptr<ResourceInitData> ResourceManager::ReadResourceInitData(const std::string& filePath,
                                                                        std::shared_ptr<File> file) {
    // Header layout (20 bytes), little-endian in practice:
    //   [0]    Endianness byte
    //   [1]    IsCustom flag
    //   [2..3] reserved
    //   [4..7] Type (FourCC)
    //   [8..11] ResourceVersion
    //   [12..19] Id
    auto init = std::make_shared<ResourceInitData>();
    auto& r = file->Reader;

    init->ByteOrder = (Endianness)r->ReadInt8();
    r->SetEndianness(init->ByteOrder);
    init->IsCustom = (bool)r->ReadInt8();
    r->ReadInt8();
    r->ReadInt8();
    init->Type = r->ReadUInt32();
    init->ResourceVersion = (int32_t)r->ReadUInt32();
    init->Id = r->ReadUInt64();
    init->Path = filePath;
    // Reader now sits at offset 20, the start of the payload.
    return init;
}

std::shared_ptr<IResource> ResourceManager::LoadResource(const std::string& filePath) {
    if (auto cached = GetCachedResource(filePath)) {
        return cached;
    }

    auto file = mArchiveManager->LoadFile(filePath);
    if (file == nullptr || file->Reader == nullptr) {
        return nullptr;
    }

    file->InitData = ReadResourceInitData(filePath, file);

    auto fit = mFactories.find(file->InitData->Type);
    if (fit == mFactories.end()) {
        return nullptr; // no factory registered for this Type
    }

    auto resource = fit->second->ReadResource(file);
    if (resource != nullptr) {
        mCache[filePath] = resource;
    }
    return resource;
}

} // namespace Ship
