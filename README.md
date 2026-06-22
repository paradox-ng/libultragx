# libultragx

A lean GameCube/Wii native runtime for N64 PC decomp ports - the GX-backed
counterpart to [libultraship](https://github.com/Kenix3/libultraship). Where
libultraship targets PC, libultragx targets **GameCube and Wii** via
devkitPPC + libogc, so Fast3D-based N64 decomp ports can run natively on the
real hardware.

Target games (all forked from their PC ports): SM64 (Ghostship), Star Fox 64
(Starship), Mario Kart 64 (SpaghettiKart), Smash 64 (BattleShip). SM64 is the
first target.

## Status

**Step 1 - pipeline bring-up.** A single spinning, vertex-colored triangle
drawn through GX, proving the devkitPPC → libogc → Docker → Dolphin loop works
end-to-end. No Fast3D yet.

## Building

The toolchain runs in Docker so nothing is installed on the host:

```sh
./build.sh          # runs `make` in the devkitpro/devkitppc image -> libultragx.dol
./build.sh clean    # remove build artifacts
```

First run pulls the `devkitpro/devkitppc` image (~1-2 GB).

## Running

Dolphin runs natively on the host (flatpak) and loads the `.dol`:

```sh
./run.sh            # boots libultragx.dol in Dolphin
```

Press **HOME** (Wii remote) or **START** (GC controller) to exit.

## Layout

```
libultragx/
├── source/        # runtime + bring-up code (main.c for now)
├── Makefile       # devkitPPC wii_rules build -> libultragx.dol
├── Dockerfile     # pinned build image (build.sh uses upstream by default)
├── build.sh       # containerized `make` wrapper
└── run.sh         # launches Dolphin on the built .dol
```

Planned: `gfx_pc.c` (Fast3D interpreter, reused), `gfx_gx.c` (Fast3D → GX, the
core work), and a libogc platform layer (VI / PAD / ASND / FAT).
