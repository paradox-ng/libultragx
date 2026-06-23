#include "fast/resource/factory/MatrixFactory.h"
#include "fast/resource/type/Matrix.h"

namespace Fast {

std::shared_ptr<Ship::IResource> MatrixFactory::ReadResource(std::shared_ptr<Ship::File> file) {
    if (file == nullptr || file->Reader == nullptr) {
        return nullptr;
    }
    auto matrix = std::make_shared<Matrix>(file->InitData);
    auto& reader = file->Reader;
    // 16 int32: the N64 fixed-point matrix (8 integer-part words then 8 fraction-
    // part words), stored verbatim - the interpreter decodes the fixed point.
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            matrix->Matrx.m[i][j] = reader->ReadInt32();
        }
    }
    return matrix;
}

} // namespace Fast
