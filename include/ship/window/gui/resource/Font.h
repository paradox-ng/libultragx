#pragma once

#include <cstddef>

#include "ship/resource/Resource.h"

namespace Ship {

#define RESOURCE_TYPE_FONT 0x464F4E54 // "FONT"

// Raw font-file bytes (e.g. a TTF) loaded from an archive. Upstream hands these to
// ImGui's font atlas; the console build strips ImGui, so the resource still loads
// but the (no-op) atlas calls ignore it.
class Font : public Resource<void> {
  public:
    using Resource::Resource;

    Font() = default;

    void* GetPointer() override {
        return Data;
    }

    size_t GetPointerSize() override {
        return DataSize;
    }

    char* Data = nullptr;
    size_t DataSize = 0;
};

} // namespace Ship
