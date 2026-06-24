#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ship/resource/ResourceType.h"
#include "ship/utils/binarytools/BinaryReader.h"

namespace tinyxml2 {
class XMLDocument;
} // namespace tinyxml2

namespace Ship {
class Archive;

#define RESOURCE_FORMAT_BINARY 0
#define RESOURCE_FORMAT_XML 1

// Parsed header at the front of every resource entry. ByteOrder is read first and
// configures the reader for the remaining (little-endian, in practice) fields.
struct ResourceInitData {
    std::shared_ptr<Archive> Parent;
    std::string Path;
    Endianness ByteOrder = Endianness::Native;
    uint32_t Type = 0;
    int32_t ResourceVersion = 0;
    uint64_t Id = 0;
    bool IsCustom = false;
    uint32_t Format = RESOURCE_FORMAT_BINARY;
};

// A loaded archive entry: the decompressed bytes and a reader over them. Reader is a
// variant (binary or XML) to match the upstream libultraship contract so a game's
// factories - which do std::get<std::shared_ptr<BinaryReader>>(file->Reader) - plug
// in unchanged. libultragx only produces the BinaryReader alternative (no XML reader
// yet); the XML alternative exists for type compatibility.
struct File {
    std::shared_ptr<std::vector<char>> Buffer;
    uint32_t BufferOffset = 0;
    std::variant<std::shared_ptr<BinaryReader>, std::shared_ptr<tinyxml2::XMLDocument>> Reader;
    std::shared_ptr<ResourceInitData> InitData;
    bool IsLoaded = false;
};

} // namespace Ship
