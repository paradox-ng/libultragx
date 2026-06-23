#pragma once

#include "ship/resource/Resource.h"
#include "fast/types.h" // Mtx

namespace Fast {

// A loaded N64 matrix resource (fixed-point Mtx). The interpreter reads it as the
// raw int32 matrix via GetResourceRawPointer when a display list G_MTX references
// it by hash.
class Matrix final : public Ship::Resource<Mtx> {
  public:
    using Resource::Resource;

    Matrix() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }

    Mtx* GetPointer() override {
        return &Matrx;
    }
    size_t GetPointerSize() override {
        return sizeof(Mtx);
    }

    Mtx Matrx{};
};

} // namespace Fast
