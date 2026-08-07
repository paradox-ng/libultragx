# gfx_gx: the GX rendering backend

`gfx_gx` is the Fast3D rendering backend for GameCube/Wii. It implements
`Fast::GfxRenderingAPI` (the interface the Fast3D interpreter calls) on top of GX,
and drives the full game.

## Components

- `source/gfx/gfx_gx_tev` - N64 color combiner `(A-B)*C+D` -> GX TEV. A general
  decoder over the real `(A,B,C,D)` inputs (`B==0 -> A*C+D`, `D==B -> lerp(B,A,C)`,
  one TEV stage each), covering SM64's combiner set. Sources map TEXEL0 -> TEXC/TEXA,
  SHADE -> RASC/RASA, PRIM and ENV -> TEV registers, COMBINED -> CPREV.
- `source/gfx/gfx_gx_tex` - packs Fast3D's decoded RGBA32 into native GX texture
  formats: `GX_TF_RGB5A3` (16-bit, lossless for the N64's dominant 16-bit RGBA), and
  `GX_TF_I4`/`I8` and `GX_TF_IA4`/`IA8` for intensity/alpha. GX block geometry differs
  per format (I4 8x8, I8/IA4 8x4, IA8/RGB5A3 4x4), which the tiling and size math
  match; getting that wrong corrupts the texture.
- `source/gfx/gfx_gx_state` - depth test/write, alpha compare (cutout), blend, cull.
- `source/gfx/gfx_gx_api.cpp` (`Fast::GfxRenderingAPIGX`) - implements the ~40
  `GfxRenderingAPI` methods: shader/combiner lookup, texture binding, the frame
  lifecycle, and `DrawTriangles`. It renders direct to screen, so the
  render-to-texture / ImGui-texture methods are inert. `lugx_create_gx_rendering_api()`
  is the factory.
- `source/gfx/gfx_gx_window.cpp` (`Fast::GfxWindowBackendGX`) - VI/GX init,
  framebuffer allocation, vsync, present pacing, and the on-screen fps/profiler
  overlay.

## The transform model (software T&L)

GX has no vertex shaders, and its fixed-function pipeline expects an affine 3x4
modelview and a 4x4 projection **separately**, with a per-batch matrix. The Fast3D
interpreter instead hands the backend object-space vertices plus a per-vertex
matrix-palette slot, where each slot is a **combined** N64 MVP (projection folded
into the modelview, carrying a perspective row). Those two models do not line up: a
combined perspective-bearing matrix fits neither GX slot.

libultragx resolves this the way the sm64-port Wii branch does - software T&L on the
CPU. The interpreter packs an interleaved float vbo, per vertex:

```
x, y, z, w,            // object-space position (4 floats)
mtxIndex,              // matrix-palette slot for this vertex (1 float)
u/32, v/32,            // per used texture tile (2 floats each)
shade.rgb [, shade.a]  // if shade used; or the vertex normal under G_LIGHTING
```

`DrawTriangles` transforms each vertex to clip space on the CPU
(`clip = obj * mtx_palette[slot]`), does near-plane clipping there
(Sutherland-Hodgman, interpolating the attributes), and feeds GX the clip-space
coordinates through a fixed pass-through perspective (identity position matrix;
`[0][0]=[1][1]=1`, `[2][2]=-n/(f-n)`, `[2][3]=-nf/(f-n)`, `[3][2]=-1`, with SM64's
`n`/`f`). GX then performs only the perspective divide and rasterization. 2D
rectangles arrive pre-transformed (their palette matrix is affine) and take the
simpler single-matrix path.

Lighting uses GX hardware: the interpreter passes vertex normals plus per-light
direction coefficients, and GX computes the clamped diffuse term into the vertex
colour channel.

## Performance

`DrawTriangles` itself is a small fraction of the frame; the interpreter's
per-triangle handler dominates. To keep it in budget, the interpreter caches the
per-triangle render-state decode (combiner options, per-tile texture setup, shader
selection, blend/depth flags) and re-derives it only when a non-drawing DL command
may have changed the state, reusing it across the run of same-state triangles that
follows. The backend likewise caches its constant matrix loads (the pass-through
projection, the identity position and normal matrices) and skips them when already
resident.

30 fps is the native rate. Frame interpolation (a `config.ini` option) renders one
interpolated in-between frame per logic tick for 60 fps motion without running the
simulation at double speed.

## Antialiasing

GX's hardware 3-sample edge antialiasing is available through the `antialiasing` option
in `config.ini` (off by default). The backend selects the AA render mode matching the
console's TV standard and scan mode, and switches the EFB to `GX_PF_RGB565_Z16` - the
three coverage samples per pixel only fit at 16-bit colour and depth. GX resolves the
samples while copying the EFB out to the display.

The tradeoff is real: an AA render mode holds half as many EFB lines (480 becomes 242 on
NTSC) and the copy stretches them back to full height, so edges get smoother while
vertical detail softens and gradients may band slightly. It costs GP time rather than
CPU, which is free in practice here because the renderer is CPU-bound. Note that the
non-AA path already renders 640x480 against the N64's native 320x240, so it is already
supersampling vertically; whether AA is a net win is a judgement call for real hardware,
as emulators do not reproduce the copy filter faithfully.

Because the EFB size varies (per TV standard, and again under AA), nothing may assume
it: the window publishes the live size, and the interpreter takes its render dimensions
from the window rather than requesting a size of its own.

## Not yet done

- Render-to-texture. The EFB-copy-to-texture path is stubbed (the game renders direct
  to the EFB), so effects that read back the framebuffer are inert.
- Hardware T&L. The transform stays on the CPU; moving it onto GX would require the
  interpreter to keep modelview and projection separate and to feed native quantized
  vertex arrays (the N64 vertices are already s16/s8). This is the largest structural
  optimization and is documented against the GX manual's viewing chapter.
