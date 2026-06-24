#pragma once

#include <cstdint>
#include <vector>

#include "ship/resource/Resource.h"

namespace Ship {

// Generic binary blob resource: the raw bytes live in Data and are exposed through
// the typed Resource interface. Register a factory against ResourceType::Blob.
class Blob final : public Resource<void> {
  public:
    using Resource::Resource;

    Blob() = default;

    void* GetPointer() override {
        return Data.empty() ? nullptr : Data.data();
    }

    size_t GetPointerSize() override {
        return Data.size();
    }

    std::vector<uint8_t> Data;
};

} // namespace Ship
