#pragma once

#include <memory>

#include "ship/resource/ResourceFactoryBinary.h"
#include "ship/resource/ResourceFactoryXML.h"
#include "ship/resource/File.h"

namespace Fast {

// Reads an OVTX vertex resource (count + N64 vertices) into a Fast::Vertex.
// gbi-side (F3DVtx); register against ResourceType Vertex (0x4F565458). The XML
// variant exists for link/type compatibility; libultragx has no XML reader so it
// returns nullptr.
class ResourceFactoryBinaryVertexV0 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class ResourceFactoryXMLVertexV0 final : public Ship::ResourceFactoryXML {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

} // namespace Fast
