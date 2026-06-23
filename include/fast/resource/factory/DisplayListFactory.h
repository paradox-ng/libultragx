#pragma once

#include <memory>

#include "ship/resource/ResourceFactory.h"
#include "ship/resource/File.h"

namespace Fast {

// Builds a Fast::DisplayList from a loaded File. The implementation pulls in the
// gbi decoder, so it is confined to DisplayListFactory.cpp; this header is
// gbi-free and can be registered with the ResourceManager from any TU.
class DisplayListFactory : public Ship::ResourceFactory {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file) override;
};

} // namespace Fast
