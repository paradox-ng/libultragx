#include "ship/resource/Resource.h"

namespace Ship {

IResource::IResource(std::shared_ptr<ResourceInitData> initData) : mInitData(std::move(initData)) {}

IResource::~IResource() = default;

bool IResource::IsDirty() {
    return mIsDirty;
}

void IResource::Dirty() {
    mIsDirty = true;
}

std::shared_ptr<ResourceInitData> IResource::GetInitData() {
    return mInitData;
}

} // namespace Ship
