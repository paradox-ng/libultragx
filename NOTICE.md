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
