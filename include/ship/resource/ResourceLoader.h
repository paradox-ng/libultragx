#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "ship/resource/ResourceFactory.h"

namespace Ship {

// Identifies a factory by (format, type, version) so several factories can serve
// the same resource type - different binary versions, or an XML variant. Matches
// the upstream libultraship key so a game's RegisterResourceFactory calls land in
// the same slots.
struct ResourceFactoryKey {
    uint32_t resourceFormat;
    uint32_t resourceType;
    uint32_t resourceVersion;

    bool operator==(const ResourceFactoryKey& o) const {
        return resourceFormat == o.resourceFormat && resourceType == o.resourceType &&
               resourceVersion == o.resourceVersion;
    }
};

struct ResourceFactoryKeyHash {
    std::size_t operator()(const ResourceFactoryKey& k) const {
        return std::hash<uint32_t>()(k.resourceFormat) ^ std::hash<uint32_t>()(k.resourceType) ^
               std::hash<uint32_t>()(k.resourceVersion);
    }
};

// Registry that maps a resource's (format, type, version) to the factory that
// deserializes it. This is the surface a game configures at startup:
//   loader->RegisterResourceFactory(factory, RESOURCE_FORMAT_BINARY, "Texture",
//                                   (uint32_t)Fast::ResourceType::Texture, 0);
// The ResourceManager owns one ResourceLoader and consults it when dispatching a
// loaded File to a factory. (Header parsing / async loading from upstream are not
// reproduced; the ResourceManager handles the header and calls GetFactory.)
class ResourceLoader {
  public:
    ResourceLoader() = default;

    bool RegisterResourceFactory(std::shared_ptr<ResourceFactory> factory, uint32_t format, std::string typeName,
                                 uint32_t type, uint32_t version);

    uint32_t GetResourceType(const std::string& typeName);

    // Returns the factory for the given key, or nullptr. On an exact-version miss
    // it retries version 0, so a resource registered version-agnostically (the
    // ResourceManager FourCC shim) still resolves.
    std::shared_ptr<ResourceFactory> GetFactory(uint32_t format, uint32_t type, uint32_t version);

  private:
    std::unordered_map<std::string, uint32_t> mResourceTypes;
    std::unordered_map<ResourceFactoryKey, std::shared_ptr<ResourceFactory>, ResourceFactoryKeyHash> mFactories;
};

} // namespace Ship
