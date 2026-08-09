#include <cstring>
#include "ship/resource/ResourceManager.h"

namespace Ship {

ResourceManager::ResourceManager()
    : mArchiveManager(std::make_shared<ArchiveManager>()), mResourceLoader(std::make_shared<ResourceLoader>()) {
}

void ResourceManager::RegisterResourceFactory(uint32_t type, std::shared_ptr<ResourceFactory> factory) {
    mResourceLoader->RegisterResourceFactory(std::move(factory), RESOURCE_FORMAT_BINARY, "", type, 0);
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
    auto r = std::get<std::shared_ptr<BinaryReader>>(file->Reader);

    init->ByteOrder = (Endianness)r->ReadInt8();
    r->SetEndianness(init->ByteOrder);
    init->IsCustom = (bool)r->ReadInt8();
    r->ReadInt8();
    r->ReadInt8();
    init->Type = r->ReadUInt32();
    init->ResourceVersion = (int32_t)r->ReadUInt32();
    init->Id = r->ReadUInt64();
    init->Path = filePath;
    // The 20 header fields above are followed by reserved space: the OTR/O2R body
    // actually begins at OTR_HEADER_SIZE (64), not 20. Factories read from here,
    // so position the reader at the true payload start. (Display lists survived
    // the wrong offset only because their decoder skips leading NOPs; the
    // vertex/light/matrix/texture factories read fixed-size structs and silently
    // pulled the reserved zero region instead -> zero verts, null lights, etc.)
    constexpr int32_t kOtrHeaderSize = 64;
    r->Seek(kOtrHeaderSize, SeekOffsetType::Start);
    return init;
}

std::shared_ptr<IResource> ResourceManager::LoadResource(const std::string& rawPath, bool /*loadExact*/,
                                                         std::shared_ptr<ResourceInitData> /*initData*/) {
    // Game resource references carry the libultraship "__OTR__" prefix (display-list
    // pointers replaced with archive paths, audio/asset lookups, etc.); strip it so
    // the name matches the archive entry.
    static const std::string kOtrPrefix = "__OTR__";
    const std::string filePath =
        rawPath.compare(0, kOtrPrefix.size(), kOtrPrefix) == 0 ? rawPath.substr(kOtrPrefix.size()) : rawPath;

    if (auto cached = GetCachedResource(filePath)) {
        return cached;
    }

    auto file = mArchiveManager->LoadFile(filePath);
    if (file == nullptr || !std::holds_alternative<std::shared_ptr<BinaryReader>>(file->Reader) ||
        std::get<std::shared_ptr<BinaryReader>>(file->Reader) == nullptr) {
        return nullptr;
    }

    file->InitData = ReadResourceInitData(filePath, file);

    auto factory = mResourceLoader->GetFactory(file->InitData->Format, file->InitData->Type,
                                               (uint32_t)file->InitData->ResourceVersion);
    if (factory == nullptr) {
        return nullptr; // no factory registered for this (format, type, version)
    }

    auto resource = factory->ReadResource(file, file->InitData);
    if (resource != nullptr) {
        mCache[filePath] = resource;
    }
    return resource;
}

std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const std::string& filePath, bool loadExact) {
    return LoadResource(filePath, loadExact);
}

void ResourceManager::LoadResources(const std::string& /*searchMask*/) {
    // No-op: resources load lazily on first use. A real preload would iterate the
    // archive entries matching the mask and LoadResource each.
}

void* ResourceManager::GetResourceRawPointer(const std::string& name) {
    auto resource = LoadResource(name);
    return resource != nullptr ? resource->GetRawPointer() : nullptr;
}

void* ResourceManager::GetResourceRawPointer(uint64_t crc) {
    const char* name = mArchiveManager->HashToCString(crc);
    return name != nullptr ? GetResourceRawPointer(std::string(name)) : nullptr;
}

void* ResourceManager::GetResourceRawPointer(std::shared_ptr<IResource> resource) {
    return resource != nullptr ? resource->GetRawPointer() : nullptr;
}

// True when `data` is a resource REFERENCE (the game tags them with a 7-char
// "__OTR__" prefix) rather than raw data. Callers (LOAD_ASSET, process_geo_layout,
// the gfx/texture middleware) use this to decide whether to resolve the reference
// through the resource manager. Returning false unconditionally left every __OTR__
// reference unresolved: e.g. process_geo_layout then ran the literal "__OTR__..."
// string as geo bytecode (cmdId '_' = 0x5F indexes past GeoLayoutJumpTable) and
// hung in an infinite dispatch loop.
bool ResourceManager::OtrSignatureCheck(const char* fileName) {
    static const char kOtrPrefix[] = "__OTR__";
    if (fileName == nullptr) {
        return false;
    }
    return std::strncmp(fileName, kOtrPrefix, sizeof(kOtrPrefix) - 1) == 0;
}

void ResourceManager::DirtyResources(const std::string& searchMask) {
    // No alt-asset switching on console: nothing to invalidate.
    (void)searchMask;
}

size_t ResourceManager::UnloadResource(const std::string& filePath) {
    (void)filePath;
    return 0;
}

} // namespace Ship
