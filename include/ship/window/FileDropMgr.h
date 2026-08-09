#pragma once
// Desktop drag-and-drop manager. Console has no file drops, so this is inert: handlers
// register successfully and simply never fire. Kept as its own header because callers
// include <ship/window/FileDropMgr.h> directly (Shipwright registers a spoiler-log
// drop handler from OTRGlobals and the randomizer).
#include <string>

namespace Ship {
typedef bool (*FileDroppedFunc)(char*);

class FileDropMgr {
  public:
    bool RegisterDropHandler(FileDroppedFunc) { return true; }
    void SetDroppedFile(std::string) {}
    std::string GetDroppedFile() { return ""; }
    bool IsFileDropped() { return false; }
    void ClearDroppedFile() {}
};
} // namespace Ship
