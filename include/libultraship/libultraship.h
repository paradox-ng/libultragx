#pragma once

// Umbrella header the games include (`#include <libultraship.h>`). libultragx
// exposes the same entry point as upstream libultraship so game source #includes do
// not change; it aggregates the N64 ABI, the C bridge/log/colour helpers, and the
// Ship:: class surface libultragx provides (see classes.h for what is and is not yet
// covered).

#include "libultra.h"
#include "bridge.h"
#include "color.h"
#include "luslog.h"
#include "classes.h"
