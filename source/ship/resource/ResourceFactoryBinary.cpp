#include "ship/resource/ResourceFactoryBinary.h"

#include <variant>

namespace Ship {

bool ResourceFactoryBinary::FileHasValidFormatAndReader(std::shared_ptr<File> file,
                                                        std::shared_ptr<ResourceInitData> initData) {
    if (file == nullptr || initData == nullptr) {
        return false;
    }
    if (initData->Format != RESOURCE_FORMAT_BINARY) {
        return false;
    }
    return std::holds_alternative<std::shared_ptr<BinaryReader>>(file->Reader);
}

} // namespace Ship
