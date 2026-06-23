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

#include <cstring>
#include <memory>
#include <unordered_map>

#include "fast/interpreter.h"
#include "fast/backends/gfx_rendering_api.h"
#include "fast/backends/gfx_window_manager_api.h"
#include "apps/interptest/scene_build.h"

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

    // Real geometry: a hand-built F3DEX2 display list (one shaded triangle). The
    // matrices are supplied as float replacements keyed by the DL's matrix
    // addresses (so the interpreter takes the float path, not fixed-point).
    LugxScene scene = lugx_build_scene_triangle();
    std::unordered_map<Mtx*, MtxF> mtxReplacements;
    MtxF projMtx;
    MtxF mvMtx;
    memcpy(projMtx.mf, scene.projF, sizeof(projMtx.mf));
    memcpy(mvMtx.mf, scene.mvF, sizeof(mvMtx.mf));
    mtxReplacements[(Mtx*)scene.projMtxAddr] = projMtx;
    mtxReplacements[(Mtx*)scene.mvMtxAddr] = mvMtx;
    std::unordered_map<Gfx*, Gfx*> dlReplacements;

    int frames = 0;
    while (true) {
        wapi->HandleEvents();
        if (!wapi->IsRunning()) {
            break;
        }
        interp->StartFrame();
        // We render direct to the EFB (framebuffers are stubbed), so keep the
        // interpreter on the direct-to-screen path instead of its render-to-
        // texture present (which our stubs cannot present).
        interp->mRendersToFb = false;
        interp->Run(reinterpret_cast<Gfx*>(scene.dl), mtxReplacements, dlReplacements);
        interp->EndFrame();
        frames++;
    }
    return frames;
}
