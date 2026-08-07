#pragma once

#include <stdbool.h>

// Boot-time settings read from a plain-text config.ini on the SD (next to the
// o2r, e.g. sd:/Ghostship/config.ini). Read once at startup and applied from the
// render pipeline. Editing the file and rebooting applies the change.

typedef enum {
    LUGX_ASPECT_AUTO = 0, // Wii: the console's 4:3/16:9 system setting; GameCube: 4:3
    LUGX_ASPECT_4_3,
    LUGX_ASPECT_16_9,
} LugxAspect;

typedef struct {
    int aspect;       // LugxAspect (default auto)
    bool fps_counter; // draw an on-screen framerate counter (default on)
    bool debug_profiler; // draw the on-screen CPU profiler rows (dev; default off)
    bool frame_interpolation; // interpolate 30fps logic to 60fps motion (default off)
    bool antialiasing; // GX 3-sample edge antialiasing (default off; see lugx_config.cpp)
} LugxConfig;

#ifdef __cplusplus
extern "C" {
#endif

// The live settings. Holds the defaults before lugx_config_load().
extern LugxConfig g_lugx_config;

// Parse the ini at `path` into g_lugx_config. Missing file or keys keep the
// defaults; a commented template is written when the file does not exist. Call
// once, after the SD is mounted.
void lugx_config_load(const char* path);

// The resolved render aspect ratio (e.g. 1.3333 or 1.7778). Handles AUTO by
// reading the Wii system setting (GameCube auto = 4:3).
float lugx_config_aspect_ratio(void);

#ifdef __cplusplus
}
#endif
