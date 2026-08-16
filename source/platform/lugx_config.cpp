#include "lugx_config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef HW_RVL
#include <ogc/conf.h>
#endif

// Defaults: aspect auto (Wii system setting; GameCube 4:3), and every on-screen
// diagnostic off, so a build that ships without a config.ini looks like a game rather
// than like a development build. The counter stays one line away for hardware testing.
LugxConfig g_lugx_config = {
    /* aspect      */ LUGX_ASPECT_AUTO,
    /* fps_counter */ false,
    /* debug_profiler */ false,
    /* frame_interpolation */ false,
    /* antialiasing */ false,
};

static const char* kTemplate =
    "# Ghostship (libultragx) settings. Edit and reboot to apply.\n"
    "\n"
    "# Display aspect ratio.\n"
    "#   auto = use the Wii's system 4:3/16:9 setting (GameCube defaults to 4:3)\n"
    "#   4:3  = force standard\n"
    "#   16:9 = force widescreen (anamorphic; looks right on a 16:9 TV)\n"
    "aspect_ratio = auto\n"
    "\n"
    "# On-screen framerate counter (top-right). Handy while testing on hardware.\n"
    "#   true | false\n"
    "fps_counter = false\n"
    "\n"
    "# On-screen CPU profiler rows under the fps counter (development diagnostic:\n"
    "# whole-frame / draw / vertex-load / per-triangle / combiner microseconds).\n"
    "#   true | false\n"
    "debug_profiler = false\n"
    "\n"
    "# Frame interpolation: render in-between frames for 60fps motion. Game logic still\n"
    "# runs at its native 30fps (costs more CPU/GPU per second).\n"
    "#   true | false\n"
    "frame_interpolation = false\n"
    "\n"
    "# Antialiasing: the console's hardware 3-sample edge antialiasing, like the N64's.\n"
    "# Smooths the jagged edges of polygons. The tradeoff is real: to fit three samples\n"
    "# per pixel the framebuffer holds half as many lines and 16-bit instead of 24-bit\n"
    "# colour, so edges get smoother while fine detail softens and gradients may band\n"
    "# slightly. Costs graphics-chip time, not CPU. Try both and keep what you prefer.\n"
    "#   true | false\n"
    "antialiasing = false\n";

// Trim leading/trailing ASCII whitespace in place, returning the start.
static char* trim(char* s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return s;
}

static bool parse_bool(const char* v, bool dflt) {
    if (!strcasecmp(v, "true") || !strcasecmp(v, "on") || !strcasecmp(v, "1") || !strcasecmp(v, "yes")) {
        return true;
    }
    if (!strcasecmp(v, "false") || !strcasecmp(v, "off") || !strcasecmp(v, "0") || !strcasecmp(v, "no")) {
        return false;
    }
    return dflt;
}

static void apply_kv(const char* key, const char* val) {
    if (!strcasecmp(key, "aspect_ratio")) {
        if (!strcasecmp(val, "auto")) {
            g_lugx_config.aspect = LUGX_ASPECT_AUTO;
        } else if (!strcasecmp(val, "4:3") || !strcasecmp(val, "4_3") || !strcasecmp(val, "43")) {
            g_lugx_config.aspect = LUGX_ASPECT_4_3;
        } else if (!strcasecmp(val, "16:9") || !strcasecmp(val, "16_9") || !strcasecmp(val, "169")) {
            g_lugx_config.aspect = LUGX_ASPECT_16_9;
        }
    } else if (!strcasecmp(key, "fps_counter")) {
        g_lugx_config.fps_counter = parse_bool(val, g_lugx_config.fps_counter);
    } else if (!strcasecmp(key, "debug_profiler")) {
        g_lugx_config.debug_profiler = parse_bool(val, g_lugx_config.debug_profiler);
    } else if (!strcasecmp(key, "frame_interpolation")) {
        g_lugx_config.frame_interpolation = parse_bool(val, g_lugx_config.frame_interpolation);
    } else if (!strcasecmp(key, "antialiasing")) {
        g_lugx_config.antialiasing = parse_bool(val, g_lugx_config.antialiasing);
    }
}

void lugx_config_load(const char* path) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        // No config yet: keep the defaults and drop a commented template so the
        // user has something to edit. A failed write is harmless.
        FILE* w = fopen(path, "w");
        if (w != NULL) {
            fwrite(kTemplate, 1, strlen(kTemplate), w);
            fclose(w);
        }
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        char* hash = strpbrk(line, "#;");
        if (hash != NULL) {
            *hash = '\0';
        }
        char* eq = strchr(line, '=');
        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        char* key = trim(line);
        char* val = trim(eq + 1);
        if (*key != '\0' && *val != '\0') {
            apply_kv(key, val);
        }
    }
    fclose(f);
}

float lugx_config_aspect_ratio(void) {
    switch (g_lugx_config.aspect) {
        case LUGX_ASPECT_16_9:
            return 16.0f / 9.0f;
        case LUGX_ASPECT_4_3:
            return 4.0f / 3.0f;
        case LUGX_ASPECT_AUTO:
        default:
#ifdef HW_RVL
            // Wii exposes the user's 4:3/16:9 system-menu setting.
            return (CONF_GetAspectRatio() == CONF_ASPECT_16_9) ? (16.0f / 9.0f) : (4.0f / 3.0f);
#else
            // GameCube has no such setting; default to 4:3.
            return 4.0f / 3.0f;
#endif
    }
}
