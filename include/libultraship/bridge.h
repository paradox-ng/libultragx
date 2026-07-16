#pragma once

// Aggregated C bridge API. libultraship also exposes resource / audio /
// controller / window / gfx / events / crashhandler bridges here; libultragx
// adds each as the corresponding subsystem is implemented. Currently: CVars.
#include "libultraship/bridge/consolevariablebridge.h"
#include "libultraship/bridge/windowbridge.h"
#include "libultraship/bridge/audiobridge.h"
#include "libultraship/bridge/gfxdebuggerbridge.h"
#include "bridge/resourcebridge.h"

// The events C bridge pulls in <ship/events/EventTypes.h>, whose DEFINE_EVENT/
// CALL_EVENT macros (and their global EventID/IEvent/EventPriority typedefs) are how a
// game drives libultragx's event system from C. Games that use it - e.g. Ghostship -
// get it here as part of the umbrella. A game that instead ships its OWN event system
// with the same generic type names - e.g. Starship's src/port/hooks - defines
// LUGX_GAME_OWNS_EVENTS to keep libultragx's competing global typedefs out of its
// translation units (its own event system, and libultragx's internally where it needs
// it, still work: libultragx's own sources include the event headers directly).
#ifndef LUGX_GAME_OWNS_EVENTS
#include "bridge/eventsbridge.h"
#endif
