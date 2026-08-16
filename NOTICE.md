# Adopted third-party headers

libultragx is an original, lean reimplementation of the libultraship API for
GameCube/Wii. The framework code (Context, resources, window, input, audio,
config, GUI, binary tools, ...) is written from scratch over libogc/GX.

A small set of **interface-only** headers is adopted verbatim, because they
define the shared N64 ABI that every N64 decomp compiles against and therefore
must be byte-identical:

- `include/libultraship/libultra.h` and `include/libultraship/libultra/*.h`
  - the N64 OS and graphics binary interface (`gbi.h`, `gu.h`, `os.h`, `rdp.h`,
    `sptask.h`, `thread.h`, ...)
- `include/fast/types.h`
  - Fast3D base types

These come from [libultraship](https://github.com/Kenix3/libultraship) (MIT
licensed) and derive from the N64 SDK / decomp lineage. They are declarations
only. Their implementations (the libultra OS calls, `osCreateThread`,
`osContInit`, DMA, timing, ...) are reimplemented in libultragx over libogc.

# Adopted Fast3D renderer (implementation reuse)

Unlike the interface headers above, libultragx **reuses the Fast3D display-list
interpreter implementation** rather than rewriting it: the gfx_pc interpreter is
the proven heart of every libultraship port, and our work is to drive it through
a new fixed-function GX/TEV backend, not to reimplement it.

Adopted from [libultraship](https://github.com/Kenix3/libultraship) (MIT), with
their copyright retained:

- `source/fast/interpreter.cpp` (the gfx_pc interpreter). Adopted as a whole and
  since **modified**, so it is no longer byte-identical to upstream. The changes are
  those the console needs: a per-triangle render-state decode cache to fit the frame
  budget, colour-index textures expanded through their palette when the backend
  cannot do a dependent lookup, palette formats the desktop path never referenced,
  and the render dimensions taken from the window rather than requested of it. The
  file remains MIT under its original copyright
- `include/fast/interpreter.h`, `lus_gbi.h`, `f3dex.h`, `f3dex2.h`,
  `ucodehandlers.h`
- `include/fast/backends/gfx_rendering_api.h`, `gfx_window_manager_api.h`
- `include/fast/resource/type/{Texture,Light,DisplayList}.h` and their `.cpp`
- `include/fast/debug/GfxDebugger.h` and `source/fast/debug/GfxDebugger.cpp`

The new code is the **GX/TEV rendering backend** (`source/gfx/gfx_gx_*`, an
original `Fast::GfxRenderingAPI` implementation) and the lean `ship/` framework
the interpreter runs on (Context, ResourceManager, archives, binary tools).

Also adopted verbatim: `source/ship/utils/StrHash64.cpp` + header (the CRC64
resource-path hash; MIT / zlib-licensed, copyrights retained in the file). The
OTR/O2R resource hashes must match this exact CRC64, so it is used as-is.
