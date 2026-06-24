// DisplayList factory (gbi side). Confined to its own TU: it pulls in the gbi
// decoder (and thus the N64 Gfx/Vtx types), which cannot share a TU with libogc
// gx.h. The ResourceManager registers this against the ODLT FourCC.

#include "fast/resource/factory/DisplayListFactory.h"
#include "fast/resource/DisplayListDecode.h"

namespace Fast {

std::shared_ptr<Ship::IResource> DisplayListFactory::ReadResource(std::shared_ptr<Ship::File> file,
                                                                  std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    return lugx_read_display_list(initData, std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader));
}

} // namespace Fast
