# libultragx build toolchain - devkitPPC + libogc for GameCube/Wii.
#
# We build inside this container so the host Manjaro install stays clean; only
# Dolphin (host flatpak) runs natively to load the resulting .dol.
#
# The upstream image already exports DEVKITPRO / DEVKITPPC and ships libogc plus
# the tools (elf2dol, gxtexconv, wiiload, ...), so step-1 needs nothing extra.
# This file exists so we can pin a version and, later, add the Python/Torch
# dependencies the game ports need for asset extraction.
#
# build.sh uses the upstream image directly by default. To use this pinned
# image instead:  docker build -t libultragx-build . && LIBULTRAGX_IMAGE=libultragx-build ./build.sh
FROM devkitpro/devkitppc:latest

WORKDIR /project
