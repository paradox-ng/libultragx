# gfx_gx: the GX rendering backend

`gfx_gx` is the Fast3D rendering backend for GameCube/Wii. It implements
`Fast::GfxRenderingAPI` (the interface the Fast3D interpreter calls) on top of GX.

## Status

**Primitives - done and visually verified on rendered frames (the gfxdemo app):**
- `source/gfx/gfx_gx_tev` - N64 color combiner `(A-B)*C+D` -> GX TEV. General
  decoder over the real `(A,B,C,D)` inputs. `B==0 -> A*C+D` and `D==B -> lerp(B,A,C)`,
  one TEV stage each; covers SM64's combiner set. Sources: TEXEL0->TEXC/TEXA,
  SHADE->RASC/RASA, PRIM->TEV reg0, ENV->TEV reg1, COMBINED->CPREV.
- `source/gfx/gfx_gx_tex` - linear RGBA32 -> `GX_TF_RGBA8` tiled (4x4 tiles, AR
  plane then GB plane). Fast3D pre-decodes all N64 texture formats to RGBA32.
- `source/gfx/gfx_gx_state` - depth test/write, alpha compare (cutout), src-alpha blend.

**Backend class - conforms to the contract, builds:**
- `Fast::GfxRenderingAPIGX` (`include/fast/backends/gfx_gx.h`,
  `source/gfx/gfx_gx_api.cpp`) implements all ~40 `GfxRenderingAPI` methods. State,
  textures (with a texture table + per-tile binding), and the frame lifecycle are
  wired to the primitives above. Renders direct to screen, so the framebuffer /
  ImGui-texture methods are inert. `lugx_create_gx_rendering_api()` is the factory.

## The remaining integration (one coupled unit, gated on the interpreter)

Two interlocking TODOs that cannot be validated until the Fast3D interpreter
compiles for GC and feeds real display lists:

### 1. Shader-id -> combiner decode
`CreateAndLoadNewShader(id0, id1)` receives LUS's 64-bit packed RDP combine state.
Decoding it yields (a) the `LugxCombiner` for TEV and (b) **which vbo fields are
present** (used textures, shade, alpha) - which `DrawTriangles` needs to parse the
buffer. So this must land with DrawTriangles.

### 2. DrawTriangles + the transform model
The interpreter packs an interleaved float vbo, per vertex:

```
x, y, z, w,            // object-space position (4 floats)
mtxIndex,              // matrix-palette slot for THIS vertex (1 float)
u/32, v/32,            // per used texture tile (2 floats each)
shade.rgb [, shade.a]  // if shade used (3 or 4 floats); or vertex normal under G_LIGHTING
```

Stride = `buf_vbo_len / (buf_vbo_num_tris * 3)`. The position transform runs in
LUS's vertex shader: each vertex is multiplied by `mtx_palette[mtxIndex]` (set via
`SetTransformUniforms`), then `y_scale` flips y.

**The crux:** `mtx_palette[slot]` is a *combined* N64 MVP (projection x modelview),
with a perspective row. GX's fixed-function pipeline expects modelview (affine 3x4
position matrix) and projection (4x4) *separately*, and the matrix index is
*per vertex*. Options to evaluate on hardware:
- **CPU transform**: multiply object pos by the palette matrix on PowerPC, then
  pass the result to GX. Handles per-vertex matrices correctly; perspective-correct
  texturing needs care (GX wants `w`, but GX positions are 3-component, so this
  needs the divided NDC + a w-restoring approach or accepting screen-linear UVs).
- **Load MVP as GX projection** (identity position matrix), letting GX do the
  perspective divide (perspective-correct UVs for free). Breaks the per-vertex
  matrix case, but most SM64 draws use a single matrix per batch.

This decision needs iteration against real geometry, so it waits for the interpreter.

## Path to the interpreter driving gfx_gx
1. Decode shader ids + implement DrawTriangles (above), iterating on the GX transform.
2. Get `src/fast/interpreter.cpp` compiling for GC - pulls in `prism` and the
   resource types (`Texture`, `DisplayList`), which is where `ResourceManager` +
   the `.otr`/`.o2r` zip reader finally land.
3. Implement `GfxWindowBackendGX : GfxWindowBackend` (VI/GX init, vsync, timing;
   mouse/keyboard no-ops) and register GX in `Fast3dWindow`'s backend selection.
4. Cross-compile Ghostship against libultragx; chase link errors to flesh out the
   remaining `ship/` framework (Window, ResourceManager, ...).
