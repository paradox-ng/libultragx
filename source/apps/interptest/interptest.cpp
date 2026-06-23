// libultragx interpreter bring-up test (GX/entry side).
//
// Creates the concrete GX window + rendering backends (which pull in libogc
// gx.h) and hands them, as the abstract Fast3D interfaces, to the interpreter
// driver in interptest_run.cpp (the gbi-side TU). Kept apart so gx.h and the
// N64 gbi never meet in one translation unit.
//
// Success is visual: the screen turns the libultragx pass-green once the
// interpreter completes frames through the GX backend.

#include "fast/backends/gfx_gx.h"
#include "fast/backends/gfx_gx_window.h"

// Defined in interptest_run.cpp (the interpreter/gbi-side TU).
int lugx_interptest_run(Fast::GfxWindowBackend* wapi, Fast::GfxRenderingAPI* rapi);

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Fast::GfxRenderingAPI* rapi = Fast::lugx_create_gx_rendering_api();
    Fast::GfxWindowBackend* wapi = Fast::lugx_create_gx_window_backend();

    lugx_interptest_run(wapi, rapi);
    return 0;
}
