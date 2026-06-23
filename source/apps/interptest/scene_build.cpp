// Builds a hand-crafted F3DEX2 display list for one shaded triangle, using the
// libultra GBI macros. Kept in its own TU: libultra/gbi.h (Gfx/Vtx/Mtx) cannot
// share a translation unit with the interpreter's lus_gbi.h (F3DGfx).

#define F3DEX_GBI_2
#include "libultraship/libultra/gbi.h"

#include "apps/interptest/scene_build.h"

#include <math.h>
#include <string.h>

// Matrix storage: only the ADDRESSES matter (the DL references them); the values
// are supplied as float mtx_replacements on the interpreter side.
static Mtx s_projMtx;
static Mtx s_mvMtx;

// Object-space triangle (s16 positions) with per-vertex colours.
static Vtx s_verts[3] = {
    { { { 0, 100, -300 }, 0, { 0, 0 }, { 255, 0, 0, 255 } } },
    { { { -100, -100, -300 }, 0, { 0, 0 }, { 0, 255, 0, 255 } } },
    { { { 100, -100, -300 }, 0, { 0, 0 }, { 0, 0, 255, 255 } } },
};

static Gfx s_dl[] = {
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPClearGeometryMode(G_LIGHTING | G_CULL_BOTH | G_FOG),
    gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsDPSetRenderMode(G_RM_OPA_SURF, G_RM_OPA_SURF2),
    gsSPMatrix(&s_projMtx, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH),
    gsSPMatrix(&s_mvMtx, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH),
    gsSPVertex(s_verts, 3, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSPEndDisplayList(),
};

LugxScene lugx_build_scene_triangle() {
    LugxScene s;
    memset(&s, 0, sizeof(s));
    s.dl = s_dl;
    s.projMtxAddr = &s_projMtx;
    s.mvMtxAddr = &s_mvMtx;

    // N64-convention perspective (the interpreter transforms clip = obj_row * M;
    // this is the transpose of the GX/GL perspective). The backend transposes the
    // captured MV*P palette back to GX's clip = M*pos when it loads the projection.
    const float fovy = 60.0f * 3.14159265f / 180.0f;
    const float cot = 1.0f / tanf(fovy * 0.5f);
    const float aspect = 4.0f / 3.0f;
    const float n = 10.0f, f = 2000.0f;
    s.projF[0][0] = cot / aspect;
    s.projF[1][1] = cot;
    s.projF[2][2] = f / (n - f);
    s.projF[3][2] = (n * f) / (n - f); // transposed off-diagonal
    s.projF[2][3] = -1.0f;             // transposed off-diagonal

    // Identity modelview.
    s.mvF[0][0] = s.mvF[1][1] = s.mvF[2][2] = s.mvF[3][3] = 1.0f;
    return s;
}
