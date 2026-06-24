#pragma once

#include "ship/resource/Resource.h"
#include "ship/resource/ResourceFactoryBinary.h"

namespace Ship {

// Binary factory for Blob resources (version 0): reads a length-prefixed raw byte
// run from the File's BinaryReader.
class ResourceFactoryBinaryBlobV0 final : public ResourceFactoryBinary {
  public:
    std::shared_ptr<IResource> ReadResource(std::shared_ptr<File> file,
                                            std::shared_ptr<ResourceInitData> initData) override;
};

} // namespace Ship
