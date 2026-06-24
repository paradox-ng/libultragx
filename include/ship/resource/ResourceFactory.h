#pragma once

#include <memory>

#include "ship/resource/Resource.h"
#include "ship/resource/File.h"

namespace Ship {

// Turns a loaded File into a typed resource. Matches the upstream libultraship
// contract so a game's factories plug in unchanged: ReadResource takes the File +
// its parsed InitData, and FileHasValidFormatAndReader gates the call. Concrete
// factories derive from ResourceFactoryBinary / ResourceFactoryXML, which implement
// FileHasValidFormatAndReader for their format. The ResourceLoader (or, for now, the
// ResourceManager) registers factories and invokes this.
class ResourceFactory {
  public:
    virtual ~ResourceFactory() = default;

    virtual std::shared_ptr<IResource> ReadResource(std::shared_ptr<File> file,
                                                    std::shared_ptr<ResourceInitData> initData) = 0;

  protected:
    virtual bool FileHasValidFormatAndReader(std::shared_ptr<File> file,
                                             std::shared_ptr<ResourceInitData> initData) = 0;
};

} // namespace Ship
