#pragma once

#include <memory>

#include "ship/resource/ResourceFactoryBinary.h"
#include "ship/resource/File.h"

namespace Fast {

// Reads an OTEX texture resource (type/size + raw N64 texture bytes) into a
// Fast::Texture. The interpreter decodes the N64 format to RGBA32 (ImportTexture*)
// before handing it to the backend's UploadTexture. Register against ResourceType
// Texture (0x4F544558). Handles binary versions 0 and 1.
class TextureFactory : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file, std::shared_ptr<Ship::ResourceInitData> initData) override;
};

} // namespace Fast
