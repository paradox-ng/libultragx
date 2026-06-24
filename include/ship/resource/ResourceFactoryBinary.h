#pragma once

#include "ship/resource/ResourceFactory.h"

namespace Ship {

// Base for factories reading a binary-format resource. Implements the format/reader
// gate; subclasses override ReadResource and pull the BinaryReader via
// std::get<std::shared_ptr<BinaryReader>>(file->Reader).
class ResourceFactoryBinary : public ResourceFactory {
  protected:
    bool FileHasValidFormatAndReader(std::shared_ptr<File> file,
                                     std::shared_ptr<ResourceInitData> initData) override;
};

} // namespace Ship
