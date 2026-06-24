#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "ship/resource/Resource.h"
#include "ship/resource/ResourceFactory.h"
#include "ship/resource/ResourceLoader.h"
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

    // The format/type/version factory registry. A game configures it directly:
    //   GetResourceLoader()->RegisterResourceFactory(factory, format, name, type, version);
    std::shared_ptr<ResourceLoader> GetResourceLoader() const { return mResourceLoader; }

    // Convenience shim: registers a factory for a Type FourCC as binary/version-0.
    // Used by libultragx's own test apps; equivalent to a RESOURCE_FORMAT_BINARY,
    // version 0 RegisterResourceFactory on the loader.
    void RegisterResourceFactory(uint32_t type, std::shared_ptr<ResourceFactory> factory);

    void SetAltAssetsEnabled(bool enabled) { mAltAssetsEnabled = enabled; }
    bool IsAltAssetsEnabled() const { return mAltAssetsEnabled; }

    std::shared_ptr<IResource> GetCachedResource(const std::string& filePath);
    std::shared_ptr<IResource> LoadResource(const std::string& filePath);

    // libultraship-compatible surface the Fast3D interpreter calls. Process =
    // load (our loader is already synchronous, so it is a thin alias). The
    // raw-pointer helpers load (if needed) and hand back the typed payload.
    std::shared_ptr<IResource> LoadResourceProcess(const std::string& filePath);
    void* GetResourceRawPointer(const std::string& name);
    void* GetResourceRawPointer(uint64_t crc);
    void* GetResourceRawPointer(std::shared_ptr<IResource> resource);
    // OTR magic-signature check. Real archives carry a signature; stubbed until
    // that path is needed (returns false = "not a raw OTR pointer").
    bool OtrSignatureCheck(const char* fileName);

  private:
    // Parse the resource header at the front of `file` (sets ByteOrder/Type/
    // Version/Id/Path), configure the reader's endianness, and leave the reader
    // positioned at the payload (offset 20).
    std::shared_ptr<ResourceInitData> ReadResourceInitData(const std::string& filePath,
                                                           std::shared_ptr<File> file);

    std::shared_ptr<ArchiveManager> mArchiveManager;
    std::shared_ptr<ResourceLoader> mResourceLoader;
    std::unordered_map<std::string, std::shared_ptr<IResource>> mCache;
    bool mAltAssetsEnabled = false;
};

} // namespace Ship
