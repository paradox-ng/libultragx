#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/type/Texture.h"

namespace Fast {

std::shared_ptr<Ship::IResource> TextureFactory::ReadResource(std::shared_ptr<Ship::File> file) {
    if (file == nullptr || file->Reader == nullptr || file->Buffer == nullptr) {
        return nullptr;
    }
    auto texture = std::make_shared<Texture>(file->InitData);
    auto& reader = file->Reader;
    const int version = file->InitData != nullptr ? file->InitData->ResourceVersion : 0;

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

} // namespace Fast
