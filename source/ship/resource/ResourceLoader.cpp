#include "ship/resource/ResourceLoader.h"

namespace Ship {

bool ResourceLoader::RegisterResourceFactory(std::shared_ptr<ResourceFactory> factory, uint32_t format,
                                             std::string typeName, uint32_t type, uint32_t version) {
    if (factory == nullptr) {
        return false;
    }
    mResourceTypes[typeName] = type;

    ResourceFactoryKey key{ format, type, version };
    if (mFactories.find(key) != mFactories.end()) {
        return false; // a factory with this exact key already exists
    }
    mFactories[key] = std::move(factory);
    return true;
}

uint32_t ResourceLoader::GetResourceType(const std::string& typeName) {
    auto it = mResourceTypes.find(typeName);
    return it != mResourceTypes.end() ? it->second : 0;
}

std::shared_ptr<ResourceFactory> ResourceLoader::GetFactory(uint32_t format, uint32_t type, uint32_t version) {
    auto it = mFactories.find(ResourceFactoryKey{ format, type, version });
    if (it != mFactories.end()) {
        return it->second;
    }
    if (version != 0) {
        // Version-agnostic fallback: a resource whose factory was registered
        // without a specific version (e.g. via the ResourceManager FourCC shim)
        // lives under version 0.
        auto z = mFactories.find(ResourceFactoryKey{ format, type, 0 });
        if (z != mFactories.end()) {
            return z->second;
        }
    }
    return nullptr;
}

} // namespace Ship
