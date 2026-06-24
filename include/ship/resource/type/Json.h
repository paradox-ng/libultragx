#pragma once

#include <cstddef>

#include "ship/resource/Resource.h"
#include <nlohmann/json.hpp>

namespace Ship {

// Parsed JSON document resource. Data holds the nlohmann::json tree; DataSize is
// the byte length of the original serialized string. Register a factory against
// ResourceType::Json.
class Json final : public Resource<void> {
  public:
    using Resource::Resource;

    Json() = default;

    void* GetPointer() override {
        return &Data;
    }

    size_t GetPointerSize() override {
        return DataSize;
    }

    nlohmann::json Data;
    size_t DataSize = 0;
};

} // namespace Ship
