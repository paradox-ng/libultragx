#include "fast/resource/factory/LightFactory.h"
#include "fast/resource/type/Light.h"

namespace Fast {

std::shared_ptr<Ship::IResource> LightFactory::ReadResource(std::shared_ptr<Ship::File> file) {
    if (file == nullptr || file->Reader == nullptr) {
        return nullptr;
    }
    auto light = std::make_shared<Light>(file->InitData);
    // The body is a single LightEntry, copied in verbatim (matches upstream).
    file->Reader->Read((char*)light->GetPointer(), (int32_t)sizeof(LightEntry));
    return light;
}

} // namespace Fast
