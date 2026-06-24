// C resource bridge: the C game code (and GBIMiddleware) reaches the ResourceManager
// through these. Wraps libultragx's lean ResourceManager; the directory/async/dirty/
// unload calls are no-ops on console (synchronous loader, no eviction needed yet).

#include "libultraship/bridge/resourcebridge.h"

#include <string>

#include "ship/Context.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/ArchiveManager.h"
#include "ship/utils/StrHash64.h"
#include "fast/resource/type/Texture.h"

static std::shared_ptr<Ship::ResourceManager> RM() {
    return Ship::Context::GetInstance()->GetResourceManager();
}

std::shared_ptr<Ship::IResource> ResourceLoad(const char* name) {
    return name != nullptr ? RM()->LoadResource(name) : nullptr;
}

std::shared_ptr<Ship::IResource> ResourceLoad(uint64_t crc) {
    const char* name = RM()->GetArchiveManager()->HashToCString(crc);
    return name != nullptr ? RM()->LoadResource(name) : nullptr;
}

extern "C" {

uint64_t ResourceGetCrcByName(const char* name) {
    return name != nullptr ? CRC64(name) : 0;
}

const char* ResourceGetNameByCrc(uint64_t crc) {
    return RM()->GetArchiveManager()->HashToCString(crc);
}

size_t ResourceGetSizeByName(const char* name) {
    auto r = ResourceLoad(name);
    return r != nullptr ? r->GetPointerSize() : 0;
}

size_t ResourceGetSizeByCrc(uint64_t crc) {
    auto r = ResourceLoad(crc);
    return r != nullptr ? r->GetPointerSize() : 0;
}

uint8_t ResourceGetIsCustomByName(const char* /*name*/) {
    return 0; // no custom/mod assets on console yet
}

uint8_t ResourceGetIsCustomByCrc(uint64_t /*crc*/) {
    return 0;
}

void* ResourceGetDataByName(const char* name) {
    return RM()->GetResourceRawPointer(std::string(name != nullptr ? name : ""));
}

void* ResourceGetDataByCrc(uint64_t crc) {
    return RM()->GetResourceRawPointer(crc);
}

uint16_t ResourceGetTexWidthByName(const char* name) {
    auto t = std::static_pointer_cast<Fast::Texture>(ResourceLoad(name));
    return t != nullptr ? t->Width : 0;
}

uint16_t ResourceGetTexWidthByCrc(uint64_t crc) {
    auto t = std::static_pointer_cast<Fast::Texture>(ResourceLoad(crc));
    return t != nullptr ? t->Width : 0;
}

uint16_t ResourceGetTexHeightByName(const char* name) {
    auto t = std::static_pointer_cast<Fast::Texture>(ResourceLoad(name));
    return t != nullptr ? t->Height : 0;
}

uint16_t ResourceGetTexHeightByCrc(uint64_t crc) {
    auto t = std::static_pointer_cast<Fast::Texture>(ResourceLoad(crc));
    return t != nullptr ? t->Height : 0;
}

size_t ResourceGetTexSizeByName(const char* name) {
    auto t = std::static_pointer_cast<Fast::Texture>(ResourceLoad(name));
    return t != nullptr ? t->ImageDataSize : 0;
}

size_t ResourceGetTexSizeByCrc(uint64_t crc) {
    auto t = std::static_pointer_cast<Fast::Texture>(ResourceLoad(crc));
    return t != nullptr ? t->ImageDataSize : 0;
}

// Synchronous loader with no eviction yet - directory/async/dirty/unload are inert.
void ResourceLoadDirectory(const char* /*name*/) {
}
void ResourceLoadDirectoryAsync(const char* /*name*/) {
}
void ResourceDirtyDirectory(const char* /*name*/) {
}
void ResourceDirtyByName(const char* /*name*/) {
}
void ResourceDirtyByCrc(uint64_t /*crc*/) {
}
void ResourceUnloadByName(const char* /*name*/) {
}
void ResourceUnloadByCrc(uint64_t /*crc*/) {
}
void ResourceUnloadDirectory(const char* /*name*/) {
}
void ResourceClearCache() {
}

void ResourceGetGameVersions(uint32_t* /*versions*/, size_t /*versionsSize*/, size_t* versionsCount) {
    if (versionsCount != nullptr) {
        *versionsCount = 0;
    }
}

uint32_t ResourceHasGameVersion(uint32_t /*hash*/) {
    return 0;
}

uint32_t IsResourceManagerLoaded() {
    return Ship::Context::GetInstance()->GetResourceManager() != nullptr ? 1 : 0;
}

} // extern "C"
