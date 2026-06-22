#include "platform/sd.h"

#include <stdbool.h>
#include <fat.h>
#ifdef HW_RVL
#include <sdcard/wiisd_io.h>
#else
#include <sdcard/gcsd.h>
#endif

static bool s_mounted = false;

const char *lugx_sd_mount(void) {
    if (s_mounted) return "already mounted";

#ifdef HW_RVL
    // Wii: front SD slot.
    if (fatMountSimple("sd", &__io_wiisd)) { s_mounted = true; return "Wii SD"; }
#else
    // GameCube: SD2SP2 (EXI serial port 2) is the primary; SD Gecko in either
    // memory card slot is the fallback for other people's setups.
    if (fatMountSimple("sd", &__io_gcsd2)) { s_mounted = true; return "SD2SP2"; }
    if (fatMountSimple("sd", &__io_gcsda)) { s_mounted = true; return "SD Gecko (slot A)"; }
    if (fatMountSimple("sd", &__io_gcsdb)) { s_mounted = true; return "SD Gecko (slot B)"; }
#endif

    return NULL;
}

void lugx_sd_unmount(void) {
    if (!s_mounted) return;
    fatUnmount("sd");
    s_mounted = false;
}
