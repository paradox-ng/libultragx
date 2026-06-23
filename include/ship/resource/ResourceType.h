#pragma once

namespace Ship {

// Generic resource type FourCCs (read from a little-endian uint32, so the bytes
// in the archive are reversed). Per-module resource types (DisplayList "ODLT",
// Texture "OTEX", etc.) are defined alongside their resource classes.
enum class ResourceType {
    None = 0x00000000,
    Blob = 0x4F424C42,   // "OBLB"
    Json = 0x4A534F4E,   // "JSON"
    Shader = 0x53484144, // "SHAD"
};

} // namespace Ship
