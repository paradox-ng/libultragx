#pragma once

#include <vector>

#include "ship/resource/Resource.h"
#include "fast/lus_gbi.h" // F3DVtx

namespace Fast {

// A loaded vertex resource: an array of N64 vertices (object-space position,
// texcoord, colour/normal). The interpreter consumes the raw F3DVtx array via
// GetResourceRawPointer when a display list references it by hash.
class Vertex final : public Ship::Resource<F3DVtx> {
  public:
    using Resource::Resource;

    Vertex() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }

    F3DVtx* GetPointer() override {
        return VertexList.data();
    }
    size_t GetPointerSize() override {
        return VertexList.size() * sizeof(F3DVtx);
    }

    std::vector<F3DVtx> VertexList;
};

} // namespace Fast
