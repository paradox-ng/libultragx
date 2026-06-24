// Fast3dWindow verification: drive the real SM64 torso DL through the same
// Ship::Window / Fast::Fast3dWindow surface the game uses (Init + per-frame
// DrawAndRunGraphicsCommands), instead of poking the interpreter + backends by hand
// like apps/realdltest. Proves the increment-2 window wrapper renders end to end.
//
// Single gbi-side TU: Fast3dWindow creates the GX backends across the link itself.

#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>

#include <libultraship.h> // the game-facing umbrella (Context/ResourceManager/archives/...)
#include "libultraship/controller/controldeck/ControlDeck.h"
#include "fast/Fast3dWindow.h"
#include "fast/backends/gfx_gx_camera.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/O2rArchive.h"
#include "fast/resource/factory/DisplayListFactory.h"
#include "fast/resource/factory/VertexFactory.h"
#include "fast/resource/factory/MatrixFactory.h"
#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/factory/LightFactory.h"
#include "platform/sd.h"

static void mat_identity(float m[4][4]) {
    memset(m, 0, sizeof(float) * 16);
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
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

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    auto window = std::make_shared<Fast::Fast3dWindow>(std::vector<std::shared_ptr<Ship::GuiWindow>>({}));
    window->Init();
    window->SetRendererUCode(ucode_f3d);

    Gfx* dl = nullptr;
    if (lugx_sd_mount() != nullptr) {
        auto rm = Ship::Context::GetInstance()->GetResourceManager();
        auto archive = std::make_shared<Ship::O2rArchive>();
        if (archive->Open("sd:/sm64.o2r")) {
            rm->GetArchiveManager()->AddArchive(archive);
            rm->RegisterResourceFactory(0x4F444C54u, std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>()); // ODLT
            rm->RegisterResourceFactory(0x4F565458u, std::make_shared<Fast::ResourceFactoryBinaryVertexV0>());       // OVTX
            rm->RegisterResourceFactory(0x4F4D5458u, std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>());        // OMTX
            rm->RegisterResourceFactory(0x4F544558u, std::make_shared<Fast::ResourceFactoryBinaryTextureV0>());       // OTEX
            rm->RegisterResourceFactory(0x46669697u, std::make_shared<Fast::ResourceFactoryBinaryLightV0>());         // LGTS
            // Load through the C resource bridge (what the game's C code uses).
            dl = (Gfx*)ResourceGetDataByName("actors/mario/mario_torso_dl");
        }
    }

    // Pure-perspective MP (view goes to the GX modelview, see gfx_gx_camera.h).
    const float fovy = 60.0f * 3.14159265f / 180.0f;
    const float cot = 1.0f / tanf(fovy * 0.5f);
    const float asp = 4.0f / 3.0f, n = 10.0f, f = 50000.0f;
    float persp[4][4], view[4][4], mpT[4][4];
    memset(persp, 0, sizeof(persp));
    persp[0][0] = cot / asp;
    persp[1][1] = cot;
    persp[2][2] = -(f + n) / (f - n);
    persp[2][3] = -2.0f * f * n / (f - n);
    persp[3][2] = -1.0f;
    mat_transpose(mpT, persp);
    mat_identity(view);
    view[2][3] = -400.0f;
    Fast::lugx_gx_set_view_matrix(view);

    // Exercise the ControlDeck read path each frame (GC pad -> OSContPad). No input
    // arrives headless, but this proves PAD_Init/PAD_ScanPads run without crashing on
    // the emulated hardware; button mapping needs a real controller to verify.
    auto deck = std::make_shared<LUS::ControlDeck>();
    uint8_t controllerBits = 0;
    deck->Init(&controllerBits);

    auto interp = window->GetInterpreterWeak().lock();
    std::unordered_map<Mtx*, MtxF> mtxRepl;
    while (window->IsRunning()) {
        window->HandleEvents();
        deck->WriteToPad(nullptr);
        if (dl != nullptr && interp != nullptr) {
            // The standalone DL sets none of these (the game would); supply them.
            interp->mRdp->combine_mode = 0x08008000ULL;                 // G_CC_SHADE
            interp->mRsp->geometry_mode |= 0x00020000u | 0x00000004u;   // G_LIGHTING | G_SHADE
            mat_identity(interp->mRsp->modelview_matrix_stack[0]);
            memcpy(interp->mRsp->MP_matrix, mpT, sizeof(mpT));
            window->DrawAndRunGraphicsCommands(dl, mtxRepl);
        } else {
            window->StartFrame();
            window->EndFrame();
        }
    }
    return 0;
}
