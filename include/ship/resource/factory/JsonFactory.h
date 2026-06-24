#pragma once

#include "ship/resource/Resource.h"
#include "ship/resource/ResourceFactoryBinary.h"

namespace Ship {

// Binary factory for Json resources (version 0): reads a NUL-terminated JSON
// string from the File's BinaryReader and parses it.
class ResourceFactoryBinaryJsonV0 final : public ResourceFactoryBinary {
  public:
    std::shared_ptr<IResource> ReadResource(std::shared_ptr<File> file,
                                            std::shared_ptr<ResourceInitData> initData) override;
};

} // namespace Ship
