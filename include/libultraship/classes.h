#pragma once

// libultragx aggregation of the Ship:: classes the games pull in via <libultraship.h>.
// Mirrors upstream libultraship/classes.h, but only the subsystems libultragx has
// landed so far; the remaining ones are listed below and added as those land. (The
// Fast3D window lives in <fast/Fast3dWindow.h> and is included by the game directly,
// exactly as upstream - keeping the gbi-only interpreter out of this header so GX
// translation units can still include <libultraship.h>.)

#include "ship/Context.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/Archive.h"
#include "ship/resource/archive/ArchiveManager.h"
#include "ship/resource/archive/O2rArchive.h"
#include "ship/resource/ResourceType.h"
#include "ship/window/Window.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/utils/binarytools/BinaryReader.h"
#include "ship/utils/binarytools/BinaryWriter.h"
#include "ship/utils/binarytools/MemoryStream.h"
#include "ship/window/gui/Gui.h"
#include "ship/window/gui/GuiElement.h"
#include "ship/window/gui/GuiWindow.h"
#include "ship/controller/controldeck/ControlDeck.h"

// TODO - remaining ship/ framework for the game's full include surface (see
// docs/INTEGRATION.md). Each is a subsequent step; until they land, a game TU that
// uses them won't compile against libultragx:
//   ship/controller/controldevice/controller/Controller.h (per-device mapping; UI)
//   ship/controller/.../keyboard/KeyboardScancodes.h
//   ship/debug/Console.h
//   ship/config/Config.h
//   ship/debug/CrashHandler.h
//   ship/audio/Audio.h (+ AudioPlayer)
