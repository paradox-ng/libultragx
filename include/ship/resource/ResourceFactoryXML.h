#pragma once

#include "ship/resource/ResourceFactory.h"

namespace Ship {

// Base for factories reading an XML-format resource. libultragx has no XML reader
// (no tinyxml2), so the gate always fails at runtime - but the class exists so a
// game's XML factories (which derive from it) compile and link. XML resources are
// not used by the console build.
class ResourceFactoryXML : public ResourceFactory {
  protected:
    bool FileHasValidFormatAndReader(std::shared_ptr<File> file,
                                     std::shared_ptr<ResourceInitData> initData) override;
};

} // namespace Ship
