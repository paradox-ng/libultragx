#include "platform/paths.h"

#include <string.h>
#include <stdio.h>

char *lugx_resolve_base_dir(const char *argv0, char *out, int outsz) {
    if (!argv0 || !argv0[0]) {
        snprintf(out, outsz, "sd:/libultragx/");
        return out;
    }

    // Split argv0 into <dir>/ and <file>, where dir keeps its trailing slash.
    const char *slash = strrchr(argv0, '/');
    const char *file  = slash ? slash + 1 : argv0;          // "Ghostship.dol"
    int dirlen = slash ? (int)(slash - argv0) + 1 : 0;       // includes the '/'

    // stem = file without its extension ("Ghostship.dol" -> "Ghostship").
    const char *dot = strrchr(file, '.');
    int stemlen = dot ? (int)(dot - file) : (int)strlen(file);

    // base = <dir><stem>/
    snprintf(out, outsz, "%.*s%.*s/", dirlen, argv0, stemlen, file);
    return out;
}
