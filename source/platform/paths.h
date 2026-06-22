#ifndef LUGX_PLATFORM_PATHS_H
#define LUGX_PLATFORM_PATHS_H

#ifdef __cplusplus
extern "C" {
#endif

// Resolves the per-game base directory from the launched .dol path (argv[0]).
// The convention is a same-named folder next to the .dol, e.g.
//   "sd:/Ghostship.dol"            -> "sd:/Ghostship/"
//   "sd:/games/Ghostship.dol"      -> "sd:/games/Ghostship/"
// Falls back to "sd:/libultragx/" when argv0 is NULL/empty (loader passed no
// path). Writes a NUL-terminated path into out (capacity outsz) and returns out.
char *lugx_resolve_base_dir(const char *argv0, char *out, int outsz);

#ifdef __cplusplus
}
#endif

#endif // LUGX_PLATFORM_PATHS_H
