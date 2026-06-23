#pragma once

#include <memory>
#include <string>

#include "ship/resource/File.h"

namespace Ship {
class ResourceManager;

// Base for every loaded resource. Holds the parsed init data and exposes the raw
// payload pointer/size; typed resources derive via Resource<T>.
class IResource {
  public:
    // Path prefix marking an "alternate assets" override (HD texture packs etc.).
    // Adopted from libultraship so the Fast3D interpreter's path handling matches.
    inline static const std::string gAltAssetPrefix = "alt/";

    IResource() = default;
    IResource(std::shared_ptr<ResourceInitData> initData);
    virtual ~IResource();

    virtual void* GetRawPointer() = 0;
    virtual size_t GetPointerSize() = 0;

    bool IsDirty();
    void Dirty();
    std::shared_ptr<ResourceInitData> GetInitData();

  private:
    std::shared_ptr<ResourceInitData> mInitData;
    bool mIsDirty = false;
};

template <class T> class Resource : public IResource {
  public:
    using IResource::IResource;
    virtual T* GetPointer() = 0;
    void* GetRawPointer() override {
        return static_cast<void*>(GetPointer());
    }
};

} // namespace Ship
