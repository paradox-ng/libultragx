#pragma once

#include <cstdint>

// A hand-built test scene: a single shaded triangle display list plus the data
// it references. Built in scene_build.cpp using the libultra GBI macros (which
// cannot share a TU with the interpreter's lus_gbi). The interpreter-side runner
// casts `dl` to Gfx* and feeds the matrices as float mtx_replacements keyed by
// the matrix addresses (the interpreter reads fixed-point matrices otherwise).
struct LugxScene {
    void* dl;            // Gfx* display list
    void* projMtxAddr;   // address the projection G_MTX command references
    void* mvMtxAddr;     // address the modelview G_MTX command references
    float projF[4][4];   // projection matrix value (for the replacement)
    float mvF[4][4];     // modelview matrix value (identity)
};

LugxScene lugx_build_scene_triangle();
