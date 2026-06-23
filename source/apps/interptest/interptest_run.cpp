// libultragx interpreter bring-up test (gbi/interpreter side, no GX).
//
// Drives the real Fast3D interpreter through full frames on hardware:
//   Init -> (StartFrame -> Run(DL) -> EndFrame) per frame.
// Each completed frame presents the GX window backend's clear colour, so a solid
// colour on screen means the interpreter ran end to end through the GX backend
// (frame lifecycle, command dispatch and buffer swap all survived on PowerPC).
// The DL is a single G_ENDDL for now; real geometry follows once DrawTriangles is
// wired.
//
// Separate TU from interptest.cpp: includes fast/interpreter.h (lus_gbi
// F3DGfx/Mtx), which cannot share a TU with libogc gx.h.

#include <memory>
#include <unordered_map>

#include "fast/interpreter.h"
#include "fast/backends/gfx_rendering_api.h"
#include "fast/backends/gfx_window_manager_api.h"

// Caches the active interpreter for the global gbi command handlers (they reach
// it via mInstance.lock()). Not declared in interpreter.h, so forward-declare.
namespace Fast {
void GfxSetInstance(std::shared_ptr<Interpreter> gfx);
}

int lugx_interptest_run(Fast::GfxWindowBackend* wapi, Fast::GfxRenderingAPI* rapi) {
    // The interpreter must be owned by a shared_ptr: the global gbi command
    // handlers reach it through a weak_ptr cached by GfxSetInstance. A raw `new`
    // leaves that weak_ptr expired and the first handler null-derefs.
    auto interp = std::make_shared<Fast::Interpreter>();
    Fast::GfxSetInstance(interp);
    interp->Init(wapi, rapi, "interptest", false, 640, 480, 0, 0);

    static Fast::F3DGfx dl[1];
    dl[0].words.w0 = (uintptr_t)0xDF000000u; // G_ENDDL (F3DEX2)
    dl[0].words.w1 = (uintptr_t)0u;
    std::unordered_map<Mtx*, MtxF> mtxReplacements;
    std::unordered_map<Gfx*, Gfx*> dlReplacements;

    int frames = 0;
    while (true) {
        wapi->HandleEvents();
        if (!wapi->IsRunning()) {
            break;
        }
        interp->StartFrame();
        interp->Run(reinterpret_cast<Gfx*>(dl), mtxReplacements, dlReplacements);
        interp->EndFrame();
        frames++;
    }
    return frames;
}
