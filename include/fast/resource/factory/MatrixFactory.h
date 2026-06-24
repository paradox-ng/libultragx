#pragma once

#include <memory>

#include "ship/resource/ResourceFactoryBinary.h"
#include "ship/resource/File.h"

namespace Fast {

// Reads an OMTX matrix resource (16 floats under GBI_FLOATS, else N64 fixed-point
// int32) into a Fast::Matrix. Register against ResourceType Matrix (0x4F4D5458).
class ResourceFactoryBinaryMatrixV0 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

} // namespace Fast
