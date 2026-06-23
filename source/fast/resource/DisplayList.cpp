#include "fast/resource/type/DisplayList.h"

namespace Fast {

DisplayList::DisplayList() : Ship::Resource<Gfx>(nullptr) {}

DisplayList::~DisplayList() = default;

Gfx* DisplayList::GetPointer() {
    return Instructions.data();
}

size_t DisplayList::GetPointerSize() {
    return Instructions.size() * sizeof(Gfx);
}

} // namespace Fast
