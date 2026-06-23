#pragma once

// gbi-free bridge (no GX/gbi types) so the interpreter-side TU can set the GX
// camera without pulling in gx.h. See gfx_gx_api.cpp for the implementation.

namespace Fast {

// Set the GX modelview (affine camera/view) that DrawTriangles loads into
// GX_PNMTX0. Only the top 3 rows are used (it must be affine). Default identity.
//
// GX applies the modelview BEFORE the projection, so the camera-back translation
// belongs here - NOT folded into the projection matrix, where GX_LoadProjectionMtx
// with GX_PERSPECTIVE silently ignores the W-row translation and computes W=-z in
// object space (which flips sign across the model and explodes the geometry).
void lugx_gx_set_view_matrix(const float m[4][4]);

} // namespace Fast
