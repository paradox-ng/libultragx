#include "ship/resource/factory/JsonFactory.h"
#include "ship/resource/type/Json.h"

namespace Ship {

std::shared_ptr<IResource> ResourceFactoryBinaryJsonV0::ReadResource(std::shared_ptr<File> file,
                                                                     std::shared_ptr<ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto json = std::make_shared<Json>(initData);
    auto reader = std::get<std::shared_ptr<BinaryReader>>(file->Reader);

    json->DataSize = file->Buffer != nullptr ? file->Buffer->size() : 0;
    // allow_exceptions=true, ignore_comments=true (matches upstream).
    json->Data = nlohmann::json::parse(reader->ReadCString(), nullptr, true, true);

    return json;
}

} // namespace Ship
