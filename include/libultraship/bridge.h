#pragma once

// Aggregated C bridge API. libultraship also exposes resource / audio /
// controller / window / gfx / events / crashhandler bridges here; libultragx
// adds each as the corresponding subsystem is implemented. Currently: CVars.
#include "libultraship/bridge/consolevariablebridge.h"
#include "libultraship/bridge/windowbridge.h"
#include "libultraship/bridge/audiobridge.h"
#include "libultraship/bridge/gfxdebuggerbridge.h"
#include "bridge/resourcebridge.h"
#include "bridge/eventsbridge.h"
