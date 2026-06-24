#pragma once

#include <memory>

#include "ship/resource/ResourceFactoryBinary.h"
#include "ship/resource/ResourceFactoryXML.h"
#include "ship/resource/File.h"

namespace Fast {

// Builds a Fast::DisplayList from a loaded File. The implementation pulls in the
// gbi decoder, so it is confined to DisplayListFactory.cpp; this header is
// gbi-free and can be registered from any TU. The XML variant exists for link/type
// compatibility with games that register it; libultragx has no XML reader so it
// returns nullptr.
class ResourceFactoryBinaryDisplayListV0 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class ResourceFactoryXMLDisplayListV0 final : public Ship::ResourceFactoryXML {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

} // namespace Fast
