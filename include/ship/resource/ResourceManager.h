#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "ship/resource/Resource.h"
#include "ship/resource/ResourceFactory.h"
#include "ship/resource/File.h"
#include "ship/resource/archive/ArchiveManager.h"

namespace Ship {

// Loads typed resources by name from the mounted archives. Synchronous and
// single-threaded (GameCube has one usable core for game work and no demand
// paging; the async/thread-pool machinery of upstream LUS is dropped). Flow:
//   GetCachedResource -> ArchiveManager::LoadFile -> parse 20-byte header into
//   ResourceInitData -> the factory registered for that Type -> cache.
class ResourceManager {
  public:
    ResourceManager();

    std::shared_ptr<ArchiveManager> GetArchiveManager() const { return mArchiveManager; }

    // One factory per resource Type (FourCC, little-endian uint32 as stored).
    void RegisterResourceFactory(uint32_t type, std::shared_ptr<ResourceFactory> factory);

    std::shared_ptr<IResource> GetCachedResource(const std::string& filePath);
    std::shared_ptr<IResource> LoadResource(const std::string& filePath);

  private:
    // Parse the resource header at the front of `file` (sets ByteOrder/Type/
    // Version/Id/Path), configure the reader's endianness, and leave the reader
    // positioned at the payload (offset 20).
    std::shared_ptr<ResourceInitData> ReadResourceInitData(const std::string& filePath,
                                                           std::shared_ptr<File> file);

    std::shared_ptr<ArchiveManager> mArchiveManager;
    std::unordered_map<uint32_t, std::shared_ptr<ResourceFactory>> mFactories;
    std::unordered_map<std::string, std::shared_ptr<IResource>> mCache;
};

} // namespace Ship
