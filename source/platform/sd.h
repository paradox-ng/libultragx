#ifndef LUGX_PLATFORM_SD_H
#define LUGX_PLATFORM_SD_H

#ifdef __cplusplus
extern "C" {
#endif

// Mounts the SD card under the "sd:" volume via libfat.
// GameCube: tries SD2SP2 first, then SD Gecko slots A and B.
// Wii: the front SD slot.
// Returns a human-readable name of the device that mounted (e.g. "SD2SP2"),
// or NULL if nothing mounted.
const char *lugx_sd_mount(void);

// Unmounts the "sd:" volume if mounted. Safe to call when not mounted.
void lugx_sd_unmount(void);

#ifdef __cplusplus
}
#endif

#endif // LUGX_PLATFORM_SD_H
