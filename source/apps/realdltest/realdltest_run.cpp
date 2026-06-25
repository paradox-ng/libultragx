// First real game geometry: load a display list from sm64.o2r and run it through
// the interpreter. The DL references vertices/matrices/textures by CRC64 hash,
// which now resolve via the ResourceManager factories. A camera matrix is
// supplied (the game normally sets the projection; a standalone DL does not).
//
// gbi-side TU (interpreter / lus_gbi); no GX here.

#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>

#include "fast/interpreter.h"
#include "fast/backends/gfx_rendering_api.h"
#include "fast/backends/gfx_window_manager_api.h"
#include "ship/Context.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/O2rArchive.h"
#include "fast/resource/factory/DisplayListFactory.h"
#include "fast/resource/factory/VertexFactory.h"
#include "fast/resource/factory/MatrixFactory.h"
#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/factory/LightFactory.h"
#include "fast/backends/gfx_gx_camera.h"
#include "platform/sd.h"

// Run the loaded DL. The OTR/O2R body begins at offset 64 (OTR_HEADER_SIZE), not
// 20; once ResourceManager seeks there, the vertex/light factories read real data
// (16 verts, a red light) instead of the reserved zero region - which previously
// collapsed all geometry to a dot and crashed on a null light. mario_torso_dl is
// F3D, pure lit geometry (no textures, no matrices): we supply the camera via
// mRsp->MP_matrix directly. Camera distance (mv[2][3]) may need tuning per model.
#define LUGX_RUN_REAL_DL 1

namespace Fast {
void GfxSetInstance(std::shared_ptr<Interpreter> gfx);
}

// BISECT debug toggles the GX backend references; the game defines them in Game.cpp,
// the standalone test provides its own (all off).
extern "C" {
int g_gx_skip_draw = 0;
int g_gx_skip_geom = 0;
int g_gx_stop_at = 0;
}

// Column-vector matrix helpers (clip = M * v).
static void mat_identity(float m[4][4]) {
    memset(m, 0, sizeof(float) * 16);
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}
static void mat_mul(float r[4][4], const float a[4][4], const float b[4][4]) {
    float t[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    memcpy(r, t, sizeof(t));
}
static void mat_transpose(float r[4][4], const float m[4][4]) {
    float t[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            t[i][j] = m[j][i];
        }
    }
    memcpy(r, t, sizeof(t));
}

int lugx_realdltest_run(Fast::GfxWindowBackend* wapi, Fast::GfxRenderingAPI* rapi) {
    // Bring the window/GX up FIRST so failures show as green-no-geometry rather
    // than exiting the .dol (which returns the console to the system menu).
    auto interp = std::make_shared<Fast::Interpreter>();
    Fast::GfxSetInstance(interp);
    // Run() calls mGfxDebugger->IsDebugging() once per DL command. Without an
    // instance it derefs null - benign on Dolphin (returns 0, but logged 100k+
    // times so it crawls) and a hard crash on real hardware. Give it a real one.
    interp->SetGfxDebugger(std::make_shared<Fast::GfxDebugger>());
    interp->Init(wapi, rapi, "realdltest", false, 640, 480, 0, 0);

    Gfx* dl = nullptr;
    if (lugx_sd_mount() != nullptr) {
        auto rm = Ship::Context::GetInstance()->GetResourceManager();
        auto archive = std::make_shared<Ship::O2rArchive>();
        if (archive->Open("sd:/libultragx/sm64.o2r")) {
            rm->GetArchiveManager()->AddArchive(archive);
            rm->RegisterResourceFactory(0x4F444C54u, std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>()); // ODLT
            rm->RegisterResourceFactory(0x4F565458u, std::make_shared<Fast::ResourceFactoryBinaryVertexV0>());       // OVTX
            rm->RegisterResourceFactory(0x4F4D5458u, std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>());        // OMTX
            rm->RegisterResourceFactory(0x4F544558u, std::make_shared<Fast::ResourceFactoryBinaryTextureV0>());       // OTEX
            rm->RegisterResourceFactory(0x46669697u, std::make_shared<Fast::ResourceFactoryBinaryLightV0>());         // LGTS
            auto dlRes = rm->LoadResource("actors/mario/mario_torso_dl");
            if (dlRes != nullptr) {
                dl = (Gfx*)dlRes->GetRawPointer();
            }
        }
    }

    // GL-convention MVP = perspective * translate(-dist), stored transposed into
    // MP (the backend transposes the captured palette + remaps z). dist/fov are
    // guesses to frame an unknown-scale model; iterate.
    const float fovy = 60.0f * 3.14159265f / 180.0f;
    const float cot = 1.0f / tanf(fovy * 0.5f);
    const float asp = 4.0f / 3.0f, n = 10.0f, f = 50000.0f;
    float persp[4][4], mv[4][4], mpT[4][4];
    memset(persp, 0, sizeof(persp));
    persp[0][0] = cot / asp;
    persp[1][1] = cot;
    persp[2][2] = -(f + n) / (f - n);
    persp[2][3] = -2.0f * f * n / (f - n);
    persp[3][2] = -1.0f;
    // Pure perspective into MP (NO view folded in): GX_LoadProjectionMtx ignores a
    // W-row translation, so a baked-in camera-back push would make GX compute
    // W=-z in object space and flip sign across the model. Supply the camera-back
    // as the GX modelview instead (applied before the projection).
    mat_transpose(mpT, persp);
    mat_identity(mv);
    mv[2][3] = -400.0f; // GX modelview: push the model away from the camera (ITERATE)
    Fast::lugx_gx_set_view_matrix(mv);
    (void)mat_mul;

    std::unordered_map<Mtx*, MtxF> mtxRepl;
    std::unordered_map<Gfx*, Gfx*> dlRepl;
    while (true) {
        wapi->HandleEvents();
        if (!wapi->IsRunning()) {
            break;
        }
        interp->mRendersToFb = false;
        interp->StartFrame();
#if LUGX_RUN_REAL_DL
        if (dl != nullptr) {
            // mario_torso_dl is F3D microcode; the interpreter defaults to F3DEX2,
            // which misreads F3D opcodes (e.g. G_ENDDL 0xB8) and runs off the end
            // of the DL into garbage. Select the matching handler table first.
            Fast::gfx_set_target_ucode(ucode_f3d);
            // The standalone DL sets NO combiner (the game sets one before calling
            // an actor DL); the default combine_mode=0 outputs 0 -> solid black.
            // G_CC_SHADE (1-cycle): rgbD=alphaD=SHADE(4) -> output the interpolated
            // per-vertex shade colour, so the geometry shows in colour. Survives
            // Run()'s SpReset (which doesn't touch combine_mode).
            interp->mRdp->combine_mode = 0x08008000ULL;
            // Enable hardware lighting: G_LIGHTING makes the interpreter pack the
            // per-vertex NORMAL into the shade slot and emit LightingUniforms (the
            // red light the DL loaded); the GX backend then lights it. G_SHADE for
            // smooth shading. The DL sets neither (the game does).
            interp->mRsp->geometry_mode |= 0x00020000u | 0x00000004u; // G_LIGHTING | G_SHADE
            // Identity modelview[0]: the interpreter transforms normals + light
            // directions by it (lighting space). Position uses MP_matrix, not this.
            mat_identity(interp->mRsp->modelview_matrix_stack[0]);
            // Bake the camera-back into the MVP so the backend's software-clip path
            // (transforms each vertex by its full palette MVP) frames the torso. MVP =
            // camera-back * perspective in the interpreter's row-vector convention:
            // rows 0-2 are the perspective's; row 3 = CAMERA_Z*P_row2 + P_row3.
            const float CAMERA_Z = -400.0f;
            float MVP[4][4];
            memcpy(MVP, mpT, sizeof(MVP));
            for (int j = 0; j < 4; j++) {
                MVP[3][j] = CAMERA_Z * mpT[2][j] + mpT[3][j];
            }
            memcpy(interp->mRsp->MP_matrix, MVP, sizeof(MVP));
            interp->Run(dl, mtxRepl, dlRepl);
        }
#else
        (void)dl;
        (void)mpT;
        (void)mtxRepl;
        (void)dlRepl;
#endif
        interp->EndFrame();
    }
    return 0;
}
