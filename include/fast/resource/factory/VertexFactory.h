#pragma once

#include <memory>

#include "ship/resource/ResourceFactory.h"
#include "ship/resource/File.h"

namespace Fast {

// Reads an OVTX vertex resource (count + N64 vertices) into a Fast::Vertex.
// gbi-side (F3DVtx); register against ResourceType Vertex (0x4F565458).
class VertexFactory : public Ship::ResourceFactory {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file) override;
};

} // namespace Fast
