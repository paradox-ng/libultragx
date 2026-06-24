#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/type/Texture.h"

namespace Fast {

namespace {
// Shared parse for both texture versions. The layout difference (V1 adds Flags +
// scale fields) is driven by initData->ResourceVersion, which the ResourceLoader
// sets from the resource header - so V0 and V1 dispatch here identically.
std::shared_ptr<Ship::IResource> ReadTexture(std::shared_ptr<Ship::File> file,
                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    auto texture = std::make_shared<Texture>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);
    const int version = initData != nullptr ? initData->ResourceVersion : 0;

    texture->Type = (TextureType)reader->ReadUInt32();
    texture->Width = (uint16_t)reader->ReadUInt32();
    texture->Height = (uint16_t)reader->ReadUInt32();
    if (version >= 1) {
        texture->Flags = reader->ReadUInt32();
        texture->HByteScale = reader->ReadFloat();
        texture->VPixelScale = reader->ReadFloat();
    }
    texture->ImageDataSize = reader->ReadUInt32();
    // The image bytes follow inline in the file buffer; keep the buffer alive and
    // point ImageData at the current read offset (do not delete[] it - mImageBuffer
    // owns it).
    texture->mImageBuffer = file->Buffer;
    texture->ImageData = reinterpret_cast<uint8_t*>(file->Buffer->data() + reader->GetBaseAddress());

    return texture;
}
} // namespace

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryTextureV0::ReadResource(std::shared_ptr<Ship::File> file,
                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData) || file->Buffer == nullptr) {
        return nullptr;
    }
    return ReadTexture(file, initData);
}

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryTextureV1::ReadResource(std::shared_ptr<Ship::File> file,
                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData) || file->Buffer == nullptr) {
        return nullptr;
    }
    return ReadTexture(file, initData);
}

} // namespace Fast
