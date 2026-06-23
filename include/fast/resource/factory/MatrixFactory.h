#pragma once

#include <memory>

#include "ship/resource/ResourceFactory.h"
#include "ship/resource/File.h"

namespace Fast {

// Reads an OMTX matrix resource (16 int32 N64 fixed-point matrix) into a
// Fast::Matrix. Register against ResourceType Matrix (0x4F4D5458).
class MatrixFactory : public Ship::ResourceFactory {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file) override;
};

} // namespace Fast
