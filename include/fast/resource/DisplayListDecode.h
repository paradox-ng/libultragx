#pragma once

#include <memory>

#include "fast/resource/type/DisplayList.h"
#include "ship/resource/File.h"
#include "ship/utils/binarytools/BinaryReader.h"

// Decode a binary DisplayList payload (the reader must be positioned just past the
// resource header, with its endianness already set from ByteOrder) into a
// Fast::DisplayList. This is the libultragx equivalent of LUS's
// ResourceFactoryBinaryDisplayListV0::ReadResource.
std::shared_ptr<Fast::DisplayList>
lugx_read_display_list(std::shared_ptr<Ship::ResourceInitData> initData,
                       std::shared_ptr<Ship::BinaryReader> reader);
