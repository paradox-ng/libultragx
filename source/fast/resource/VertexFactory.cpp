// Vertex factory (gbi side - pulls F3DVtx via Vertex.h). Confined to its own TU
// like the other gbi-using factories.

#include "fast/resource/factory/VertexFactory.h"
#include "fast/resource/type/Vertex.h"

namespace Fast {

std::shared_ptr<Ship::IResource> VertexFactory::ReadResource(std::shared_ptr<Ship::File> file) {
    if (file == nullptr || file->Reader == nullptr) {
        return nullptr;
    }
    auto vertex = std::make_shared<Vertex>(file->InitData);
    auto& reader = file->Reader;

    uint32_t count = reader->ReadUInt32();
    vertex->VertexList.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        F3DVtx data{};
        data.v.ob[0] = reader->ReadInt16();
        data.v.ob[1] = reader->ReadInt16();
        data.v.ob[2] = reader->ReadInt16();
        data.v.flag = reader->ReadUInt16();
        data.v.tc[0] = reader->ReadInt16();
        data.v.tc[1] = reader->ReadInt16();
        data.v.cn[0] = reader->ReadUByte();
        data.v.cn[1] = reader->ReadUByte();
        data.v.cn[2] = reader->ReadUByte();
        data.v.cn[3] = reader->ReadUByte();
        vertex->VertexList.push_back(data);
    }
    return vertex;
}

} // namespace Fast
