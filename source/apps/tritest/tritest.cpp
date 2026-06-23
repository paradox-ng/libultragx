// libultragx DrawTriangles bring-up test (piece 4).
//
// Pure GX translation unit - no interpreter, no gbi. Brings up the GX window +
// rendering backends directly and renders one Gouraud-shaded triangle each frame
// through the real GfxRenderingAPIGX::DrawTriangles + TEV combiner path. A
// red/green/blue triangle on the green clear means the combiner decode and GX
// vertex submission work. Press START (GC) / HOME (Wii) to exit.

#include "fast/backends/gfx_gx.h"
#include "fast/backends/gfx_gx_window.h"

#include <gccore.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Fast::GfxWindowBackendGX wapi;
    Fast::GfxRenderingAPIGX rapi;

    wapi.Init("tritest", "GX", false, 640, 480, 0, 0);
    rapi.Init();

    while (true) {
        wapi.HandleEvents();
        if (!wapi.IsRunning()) {
            break;
        }
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) {
            break;
        }
#ifdef HW_RVL
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) {
            break;
        }
#endif
        // Draw into the EFB, then present (SwapBuffersEnd copies EFB->XFB and
        // re-clears the EFB to the window backend's green for the next frame).
        rapi.DrawBringupTriangle();
        wapi.SwapBuffersBegin();
        wapi.SwapBuffersEnd();
    }
    return 0;
}
