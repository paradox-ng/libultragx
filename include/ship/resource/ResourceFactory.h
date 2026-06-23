#pragma once

#include <memory>

#include "ship/resource/Resource.h"
#include "ship/resource/File.h"

namespace Ship {

// Turns a loaded File into a typed resource. The ResourceManager fills the
// File's InitData (parsed header) and leaves its Reader positioned at the start
// of the payload before calling ReadResource. One factory is registered per
// resource Type (FourCC). Factories that decode N64 data (e.g. display lists)
// live in gbi-only translation units; this base stays free of gbi/GX so the
// manager can hold factories without pulling either in.
class ResourceFactory {
  public:
    virtual ~ResourceFactory() = default;
    virtual std::shared_ptr<IResource> ReadResource(std::shared_ptr<File> file) = 0;
};

} // namespace Ship
