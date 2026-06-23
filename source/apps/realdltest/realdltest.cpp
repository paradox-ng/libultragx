// Real-DL test (GX/entry side): create the GX backends and hand them to the
// interpreter-side driver, which loads + runs a display list from sm64.o2r.

#include "fast/backends/gfx_gx.h"
#include "fast/backends/gfx_gx_window.h"

int lugx_realdltest_run(Fast::GfxWindowBackend* wapi, Fast::GfxRenderingAPI* rapi);

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Fast::GfxRenderingAPI* rapi = Fast::lugx_create_gx_rendering_api();
    Fast::GfxWindowBackend* wapi = Fast::lugx_create_gx_window_backend();

    lugx_realdltest_run(wapi, rapi);
    return 0;
}
