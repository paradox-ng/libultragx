#include "fast/resource/factory/LightFactory.h"
#include "fast/resource/type/Light.h"

namespace Fast {

std::shared_ptr<Ship::IResource> ResourceFactoryBinaryLightV0::ReadResource(std::shared_ptr<Ship::File> file,
                                                                            std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    auto light = std::make_shared<Light>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);
    // The body is a single LightEntry, copied in verbatim (matches upstream).
    reader->Read((char*)light->GetPointer(), (int32_t)sizeof(LightEntry));
    return light;
}

} // namespace Fast
