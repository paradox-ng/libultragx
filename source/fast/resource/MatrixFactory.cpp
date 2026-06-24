#include "fast/resource/factory/MatrixFactory.h"
#include "fast/resource/type/Matrix.h"

namespace Fast {

std::shared_ptr<Ship::IResource> MatrixFactory::ReadResource(std::shared_ptr<Ship::File> file,
                                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    auto matrix = std::make_shared<Matrix>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);
    // The OMTX matches the build's GBI convention: 16 floats under GBI_FLOATS (which
    // the game compiles with), else the N64 fixed-point int32 matrix. Mtx is MtxF or
    // the fixed-point form accordingly, so write the matching union member.
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
#ifdef GBI_FLOATS
            matrix->Matrx.mf[i][j] = reader->ReadFloat();
#else
            matrix->Matrx.m[i][j] = reader->ReadInt32();
#endif
        }
    }
    return matrix;
}

} // namespace Fast
