#pragma once

#include <memory>

#include "ship/resource/ResourceFactory.h"
#include "ship/resource/File.h"

namespace Fast {

// Reads an LGTS light resource (a single LightEntry: 8-byte ambient + 16-byte
// diffuse, read verbatim) into a Fast::Light. The interpreter's G_MOVEMEM
// handler points at &light->Ambient (+8 for the diffuse) when a display list
// loads light slots. Register against ResourceType Light (0x46669697).
class LightFactory : public Ship::ResourceFactory {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file) override;
};

} // namespace Fast
