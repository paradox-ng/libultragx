#include "ship/resource/ResourceFactoryXML.h"

namespace Ship {

bool ResourceFactoryXML::FileHasValidFormatAndReader(std::shared_ptr<File> /*file*/,
                                                     std::shared_ptr<ResourceInitData> initData) {
    // libultragx has no XML reader; an XML resource never validates here. The class
    // exists only so the game's XML factories compile/link (XML assets are unused).
    return initData != nullptr && initData->Format == RESOURCE_FORMAT_XML && false;
}

} // namespace Ship
